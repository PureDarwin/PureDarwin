/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
//#define IOFB_DISABLEFB 1

#include <IOKit/IOLib.h>
#include <libkern/c++/OSContainers.h>
#include <libkern/OSByteOrder.h>

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IODeviceTreeSupport.h>

#include <IOKit/i2c/IOI2CInterface.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#include <IOKit/acpi/IOACPIPlatformExpert.h>
#pragma clang diagnostic pop

#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>

#include <stdatomic.h>
#include <string.h>
#include <IOKit/assert.h>
#include <sys/kdebug.h>
#include <sys/queue.h>

#include <mach/mach_types.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#define IOFRAMEBUFFER_PRIVATE
#include <IOKit/graphics/IOGraphicsPrivate.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/graphics/IODisplay.h>

#include "IOFramebufferUserClient.h"
#include "IODisplayWrangler.h"
#include "IOFramebufferReallyPrivate.h"

#include "IOGraphicsDiagnose.h"
#include "IOGraphicsKTrace.h"
#include "GMetric/GMetric.hpp"

#ifndef kServerAckProtocolGraphicsTypesRev
#define kServerAckProtocolGraphicsTypesRev UINT32_MAX
#endif

#ifndef DBG_IOG_SOURCE_VENDOR
#define DBG_IOG_SOURCE_VENDOR 0
#endif
#ifndef DBG_IOG_SOURCE_SET_TRANSFORM
#define DBG_IOG_SOURCE_SET_TRANSFORM 0
#endif
#ifndef DBG_IOG_SOURCE_SET_ATTR_FOR_CONN_EXT
#define DBG_IOG_SOURCE_SET_ATTR_FOR_CONN_EXT 0
#endif
#ifndef DBG_IOG_SOURCE_OVERSCAN
#define DBG_IOG_SOURCE_OVERSCAN 0
#endif

#if ENABLE_TELEMETRY
#warning "**KTRACE TELEMETRY ENABLED**"
#endif


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef VERSION_MAJOR
#error no VERSION_MAJOR
#endif

#define SINGLE_THREAD	0
#define TIME_LOGS		RLOG1
#define TIME_CURSOR		RLOG1
#define ASYNC_GAMMA		1

#define GAMMA_ADJ		0

#define DOANIO        0
#define VRAM_SAVE     1
#define VRAM_COMPRESS 1
#define DEADLOCK_DETECT RLOG

enum { kIOFBVRAMCompressSpeed = 0 };
enum { kVBLThrottleTimeMS     = 5000 };
enum { kInitFBTimeoutNS = 1000000000ULL };
enum { k2xDPI = (150*10) };

//#define AUTO_COLOR_MODE		kIODisplayColorModeRGB
#define AUTO_COLOR_MODE		kIODisplayColorModeYCbCr444

#if defined(__i386__) || defined(__x86_64__)
enum { kIOFBMapCacheMode = kIOMapWriteCombineCache }; 
#else
enum { kIOFBMapCacheMode = kIOMapInhibitCache };
#endif

#ifndef kBootArgsFlagBlackBg
#define kBootArgsFlagBlackBg		(1 << 6)
#endif

#ifndef kIOPMUserTriggeredFullWakeKey
#define kIOPMUserTriggeredFullWakeKey       "IOPMUserTriggeredFullWake"
#endif

#ifndef kIOFBRedGammaScale
#define kIOFBRedGammaScale    "IOFBRedGammaScale"
#endif
#ifndef kIOFBGreenGammaScale
#define kIOFBGreenGammaScale  "IOFBGreenGammaScale"
#endif
#ifndef kIOFBBlueGammaScale
#define kIOFBBlueGammaScale   "IOFBBlueGammaScale"
#endif

#if VRAM_COMPRESS
#include "bmcompress.h"
#endif

#if DOANIO
#include <sys/uio.h>
#include <sys/conf.h>
#endif

enum {
    kIOFBClamshellProbeDelayMS = 1*1000
};
enum {
    kIOFBClamshellEnableDelayMS = 15*1000
};

enum
{
	// all seconds:
	kSystemWillSleepTimeout = 90,   // 90
	kServerAckTimeout       = 25,	// 25
	kPowerStateTimeout      = 45,
	kDarkWokeTimeout        = 5,
};

enum
{
	kIOFBEventCaptureSetting     = 0x00000001,
    kIOFBEventDisplayDimsSetting = 0x00000002,
	kIOFBEventReadClamshell	 	 = 0x00000004,
    kIOFBEventResetClamshell	 = 0x00000008,
    kIOFBEventEnableClamshell    = 0x00000010,
    kIOFBEventProbeAll			 = 0x00000020,
    kIOFBEventDisplaysPowerState = 0x00000040,
    kIOFBEventSystemPowerOn      = 0x00000080,
    kIOFBEventVBLMultiplier      = 0x00000100,
};


enum
{
    fg    = 1,
    bg    = 2,
    fgOff = 3,
    bgOff = 4,
};

enum {
    kIOFBNotifyGroupIndex_Legacy                        = 0,
    kIOFBNotifyGroupIndex_IODisplay                     = 1,
    kIOFBNotifyGroupIndex_AppleGraphicsControl          = 2,
    kIOFBNotifyGroupIndex_AppleGraphicsPowerManagement  = 3,
    kIOFBNotifyGroupIndex_AppleHDAController            = 4,
    kIOFBNotifyGroupIndex_AppleIOAccelDisplayPipe       = 5,
    kIOFBNotifyGroupIndex_AppleMCCSControl              = 6,
    kIOFBNotifyGroupIndex_VendorIntel                   = 7,
    kIOFBNotifyGroupIndex_VendorNVIDIA                  = 8,
    kIOFBNotifyGroupIndex_VendorAMD                     = 9,
    kIOFBNotifyGroupIndex_ThirdParty                    = 10,
    // Must be last
    kIOFBNotifyGroupIndex_LastIndex                     = kIOFBNotifyGroupIndex_ThirdParty,
    kIOFBNotifyGroupIndex_NumberOfGroups                = (kIOFBNotifyGroupIndex_LastIndex + 1)
};

// Clock converter helpers
static inline uint64_t ns2at(const uint64_t ns)
{
    uint64_t absolute_time;
    nanoseconds_to_absolutetime(ns, &absolute_time);
    return absolute_time;
}

static inline uint64_t at2ns(const uint64_t absolute_time)
{
    uint64_t ns;
    absolutetime_to_nanoseconds(absolute_time, &ns);
    return ns;
}

static inline uint64_t ms2at(const uint64_t ms)
{
    return ns2at(ms * kMillisecondScale);
}

static inline uint64_t at2ms(const uint64_t absolute_time)
{
    return at2ns(absolute_time) / kMillisecondScale;
}

static const uint64_t kNOTIFY_TIMEOUT_AT = ns2at(kNOTIFY_TIMEOUT_NS);
static const uint64_t kTIMELOCK_TIMEOUT_AT = ms2at(5);

#if RLOG1
static const char * processConnectChangeModeNames[] =
	{ "", "fg", "bg", "fgOff", "bgOff" };
#endif

#define CHAR(c)    ((c) ? ((char) (c)) : '0')
#define FEAT(f)    CHAR(f>>24), CHAR(f>>16), CHAR(f>>8), CHAR(f>>0)

#define API_ATTRIB_SET(inst, bit)   do{\
    OSBitOrAtomic((bit), &((inst)->__private->fAPIState));\
}while(0)
#define API_ATTRIB_CLR(inst, bit)   do{\
    OSBitAndAtomic(~(bit), &((inst)->__private->fAPIState));\
}while(0)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Static Global Variables
static class IOGraphicsWorkLoop * gIOFBSystemWorkLoop;
static STAILQ_HEAD(, IOFBController) gIOFBAllControllers
    = STAILQ_HEAD_INITIALIZER(gIOFBAllControllers);

static OSArray *            gAllFramebuffers;
static OSArray *            gStartedFramebuffers;
static IOWorkLoop *         gIOFBHIDWorkLoop;
static IOTimerEventSource * gIOFBDelayedPrefsEvent;
static IOTimerEventSource * gIOFBServerAckTimer;
static IONotifier *         gIOFBRootNotifier;
static IONotifier *         gIOFBClamshellNotify;
static IONotifier *         gIOFBGCNotifier;
static IOInterruptEventSource * gIOFBWorkES;
static atomic_uint_fast32_t gIOFBGlobalEvents;
static IORegistryEntry *    gChosenEntry;  // For kIOScreenLockStateKey property
static IOService *          gIOFBSystemPowerAckTo;
static void *               gIOFBSystemPowerAckRef;
static IOService *          gIOFBSystemPowerMuxAckTo;
static uint32_t             gIOFBSystemPowerMuxAckRef;
static UInt32               gIOFBLastMuxMessage = kIOMessageSystemHasPoweredOn;
static bool					gIOFBIsMuxSwitching;
bool                        gIOFBSystemPower = true;
bool						gIOFBSystemDark;
static bool					gIOFBServerInit;
static bool					gIOFBCurrentWSPowerOn = true;
static bool					gIOFBPostWakeNeeded;
static bool					gIOFBProbeCaptured;
static uint32_t             gIOFBCaptureState;
bool                        gIOGraphicsSystemPower = true;
static thread_call_t        gIOFBClamshellCallout;


/*! External display count. */
static SInt32               gIOFBDisplayCount;
static SInt32               gIOFBLastDisplayCount;

/*! Internal display count. */
static SInt32               gIOFBBacklightDisplayCount;

static IOOptionBits         gIOFBClamshellState;
static IOFramebuffer *      gIOFBConsoleFramebuffer;
/*!
 For when getPowerState() doesn't return what we want, e.g. in the case where
 the Doze state is not being captured by all framebuffers and we need to know
 the current state from a framebuffer that didn't update its state.
 */
static uint8_t              gIOFBPowerState = 0;

/*! When true, clamshell machines with external display keep running with lid
 *  closed; otherwise, clamshell always sleeps system. */
static bool                 gIOFBDesktopModeAllowed = true;

IOOptionBits                gIOFBCurrentClamshellState;
static IOOptionBits         gIOFBLastClamshellState;
int32_t                     gIOFBHaveBacklight = -1;
static IOOptionBits         gIOFBLastReadClamshellState;
const OSSymbol *            gIOFBGetSensorValueKey;
const OSSymbol *            gIOFramebufferKey;
const OSSymbol *            gIOFBRotateKey;
const OSSymbol *            gIOFBStartupModeTimingKey;
const OSSymbol *            gIOFBPMSettingDisplaySleepUsesDimKey;
const OSSymbol *            gIOFBConfigKey;
const OSSymbol *            gIOFBModesKey;
const OSSymbol *            gIOFBModeIDKey;
const OSSymbol *            gIOFBModeDMKey;
static OSDictionary *       gIOFBPrefs;
static OSDictionary *       gIOFBPrefsParameters;
static OSDictionary *       gIOFBIgnoreParameters;
static OSSerializer *       gIOFBPrefsSerializer;
static IOService *          gIOGraphicsControl;
static OSObject *           gIOResourcesAppleClamshellState;
static AbsoluteTime         gIOFBNextProbeAllTime;
static AbsoluteTime         gIOFBMaxVBLDelta;

void IOFramebuffer::triggerEvent(unsigned event)
{
    atomic_fetch_or(&gIOFBGlobalEvents, event);
    if (gIOFBWorkES)
        gIOFBWorkES->interruptOccurred(0, 0, 0);
}

unsigned IOFramebuffer::clearEvent(unsigned event)
{
    return atomic_fetch_and(&gIOFBGlobalEvents, ~event);
}
OSData *                    gIOFBZero32Data;
OSData *                    gIOFBOne32Data;
uint32_t             		gIOFBGrayValue = kIOFBBootGrayValue;
OSData *                    gIOFBGray32Data;
static uint8_t				gIOFBBlackBoot;
static uint8_t				gIOFBBlackBootTheme;
static uint8_t				gIOFBVerboseBoot;
static uint32_t             gIOFBOpenGLMask;
static const OSSymbol *     gIOGraphicsPrefsVersionKey;
static OSNumber *           gIOGraphicsPrefsVersionValue;

/*! When true (default), opening the lid causes the internal display to be
 *  detected & brought back online; when clear, the lid can be opened without
 *  the internal display being brought back online, which was the expected
 *  behavior before Lion. Available to power users via boot-args.
 *  (rdar://6682885) */
static uint8_t				gIOFBLidOpenMode;

static uint8_t				gIOFBVBLThrottle;
static uint8_t				gIOFBVBLDrift;
atomic_uint_fast64_t		gIOGDebugFlags;
uint32_t					gIOGNotifyTO;
bool                        gIOGFades;
static uint64_t				gIOFBVblDeltaMult;
bool                        gIOFBSetPreviewImage;

static const IOSelect gForwardGroup[kIOFBNotifyGroupIndex_NumberOfGroups] = {
    kIOFBNotifyGroupIndex_IODisplay,
    kIOFBNotifyGroupIndex_AppleMCCSControl,
    kIOFBNotifyGroupIndex_AppleHDAController,
    kIOFBNotifyGroupIndex_AppleIOAccelDisplayPipe,
    kIOFBNotifyGroupIndex_AppleGraphicsControl,
    kIOFBNotifyGroupIndex_AppleGraphicsPowerManagement,
    kIOFBNotifyGroupIndex_Legacy,
    kIOFBNotifyGroupIndex_VendorIntel,
    kIOFBNotifyGroupIndex_VendorNVIDIA,
    kIOFBNotifyGroupIndex_VendorAMD,
    kIOFBNotifyGroupIndex_ThirdParty };

static const IOSelect gReverseGroup[kIOFBNotifyGroupIndex_NumberOfGroups] = {
    kIOFBNotifyGroupIndex_AppleGraphicsPowerManagement,
    kIOFBNotifyGroupIndex_AppleGraphicsControl,
    kIOFBNotifyGroupIndex_AppleIOAccelDisplayPipe,
    kIOFBNotifyGroupIndex_AppleHDAController,
    kIOFBNotifyGroupIndex_AppleMCCSControl,
    kIOFBNotifyGroupIndex_IODisplay,
    kIOFBNotifyGroupIndex_Legacy,
    kIOFBNotifyGroupIndex_VendorIntel,
    kIOFBNotifyGroupIndex_VendorNVIDIA,
    kIOFBNotifyGroupIndex_VendorAMD,
    kIOFBNotifyGroupIndex_ThirdParty };

#if DEBG_CATEGORIES_BUILD
#warning **LOGS**
uint64_t gIOGraphicsDebugCategories = DEBG_CATEGORIES_RUNTIME_DEFAULT;
SYSCTL_QUAD(_debug, OID_AUTO, iogdebg, CTLFLAG_RW, &gIOGraphicsDebugCategories, "");
SYSCTL_QUAD(_debug, OID_AUTO, iogdebugflags, CTLFLAG_RW, &gIOGDebugFlags, "");
#endif

uint32_t                    gIOGATFlags;
uint32_t                    gIOGATLines;
GTraceBuffer::shared_type   gGTrace;
GTraceBuffer::shared_type   gAGDCGTrace;
GMetricsRecorder            *gGMetrics = NULL;

// console clut
extern UInt8 appleClut8[256 * 3];

namespace {
// TODO(gvdl) Move all static variables into an anonymous namespace
GTraceBuffer::shared_type   sAGDCGTrace;

#define kIOFBGetSensorValueKey  "getSensorValue"

enum { kIOFBDefaultScalerUnderscan = 0*kIOFBScalerUnderscan };
enum {
    kIOFBControllerMaxFBs = 32,
};

// IOFBController fStates
enum {
    kIOFBMuted     = 0x01,
    kIOFBNotOpened = 0x02,  // Controller has no opened FBs
    kIOFBAccelProbed = 0x04, // IOAccelerator probe delivered
};

};  // namespace

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct IOFBController : public OSObject
{
    OSDeclareDefaultStructors(IOFBController);
private:
    using super = OSObject;

public:
    enum CopyType { kLookupOnly, kDepIDCreate, kForceCreate };

    STAILQ_ENTRY(IOFBController) fNextController;
    IOFramebuffer  *             fFbs[kIOFBControllerMaxFBs + 1];
    class IOGraphicsWorkLoop *   fWl;
    IOInterruptEventSource *     fWorkES;
    IOService      *             fDevice;
    OSNumber       *             fDependentID;
    const char     *             fName;
    AbsoluteTime                 fInitTime;

    thread_t                     fPowerThread;

    uint32_t                     fVendorID;
    uint32_t                     fMaxFB;
    uint32_t                     fOnlineMask;

    int32_t                      fConnectChange;
    int32_t                      fLastForceRetrain;
    int32_t                      fLastMessagedChange;
    int32_t                      fLastFinishedChange;
    int32_t                      fPostWakeChange;
    bool                         fConnectChangeForMux;
    bool                         fMuxNeedsBgOn;
    bool                         fMuxNeedsBgOff;

    uint8_t                      fState;

    uint8_t                      fWsWait;

    uint8_t                      fNeedsWork;
    uint8_t                      fDidWork;
    uint8_t                      fAsyncWork;
    uint8_t                      fPendingMuxPowerChange;
    uint8_t                      fIntegrated;
    uint8_t                      fExternal:1;

    uint32_t                     fComputedState;
    uint32_t                     fAliasID;
#if IOFB_DISABLEFB
    uintptr_t                    fSaveGAR;
#endif

    char                         * fDGPUName;
    size_t                       fDGPUNameLen;

    static IOFBController *withFramebuffer(IOFramebuffer *fb);
    static IOFBController *copyController(IOFramebuffer *fb,
                                          const CopyType flag);
    virtual void free() APPLE_KEXT_OVERRIDE;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
    bool init(IOFramebuffer *fb);
#pragma clang diagnostic pop

    void unhookFB(IOFramebuffer *fb);

    bool isMuted() const { return fState & kIOFBMuted; }
    void setState(uint8_t bit) { fState |= bit; }
    void clearState(uint8_t bit) { fState &= ~bit; }

    // Inline functions no particular locking requirements.
    IOFBController *copy() { retain(); return this; }
    thread_t setPowerThread(thread_t thread)
    {
        thread_t ret = fPowerThread;
        fPowerThread = thread;
        return ret;
    }
    thread_t setPowerThread() { return setPowerThread(current_thread()); }
    bool onPowerThread() { return current_thread() == fPowerThread; }

    // System WL APIs
    IOFBController *alias(const char * fn);
    uint32_t computeState();

    // controller work loop (this->fWl) APIs
    void startThread();
    void didWork(IOOptionBits work);
    void asyncWork(IOInterruptEventSource *, int);
    IOOptionBits checkPowerWork(IOOptionBits state);
    IOOptionBits checkConnectionWork(IOOptionBits state);
    void messageConnectionChange();

    // Both System and Controller work loops
    void startAsync(uint32_t asyncWork);
};

struct IOFBInterruptRegister
{
    IOFBInterruptProc           handler;
    OSObject *                  target;
    void *                      ref;
    UInt32						state;
};
enum
{
	kIOFBMCCSInterruptRegister = 0,
	kIOFBNumInterruptRegister  = 1
};

struct IOFramebufferPrivate
{
    IOFBController *            controller;
    IODisplay *					display;
    IOFramebuffer *				nextMirror;
    uint64_t					regID;
	uint32_t					displayOptions;
    uint32_t					controllerIndex;
    uint32_t                    openGLIndex;
    IOGSize                     maxWaitCursorSize;
    UInt32                      numCursorFrames;
    uint32_t                    cursorBytesPerPixel;
    UInt8 *                     cursorFlags;
    volatile unsigned char **   cursorImages;
    volatile unsigned char **   cursorMasks;
    IOMemoryDescriptor *        saveBitsMD[kIOPreviewImageCount];

	IOGBounds					screenBounds[2];			// phys & virtual bounds

    class IOFramebufferParameterHandler * paramHandler;

    void *              		vblInterrupt;
    void *              		connectInterrupt;
	IOTimerEventSource *        vblUpdateTimer;
	IOTimerEventSource *        deferredCLUTSetTimerEvent;
	IOInterruptEventSource *    deferredVBLDisableEvent;
    uint64_t					actualVBLCount;
	OSObject *                  displayAttributes;

	IOFBInterruptRegister		interruptRegisters[kIOFBNumInterruptRegister];
	
    OSArray *                   cursorAttributes;
    IOFBCursorControlAttribute  cursorControl;
    IOInterruptEventSource *    cursorThread;
    IOOptionBits                cursorToDo;
    UInt32                      framePending;
    SInt32                      xPending;
    SInt32                      yPending;
    IOGPoint                    cursorHotSpotAdjust[2];
    IOGPoint                    lastHotSpot;
    void *                      waitVBLEvent;

    IOByteCount                 gammaHeaderSize;
    UInt32                      desiredGammaDataWidth;
    UInt32                      desiredGammaDataCount;

    IOInterruptEventSource *    deferredCLUTSetEvent;
    IOInterruptEventSource *    deferredSpeedChangeEvent;
    IOTimerEventSource *        delayedConnectInterrupt;
    UInt32                      delayedConnectTime;

    IOTimerEventSource *        dpInterruptES;
    void *                      dpInterruptRef;
    UInt32                      dpInterrupDelayTime;
    IOInterruptEventSource *    fCloseWorkES;
    bool                        fClosePending;


	const OSSymbol *			displayPrefKey;

    IOByteCount                 gammaDataLen;
    UInt8 *                     gammaData;
    UInt32                      gammaChannelCount;
    UInt32                      gammaDataCount;
    UInt32                      gammaDataWidth;

    IOByteCount                 rawGammaDataLen;
    UInt8 *                     rawGammaData;
    UInt32                      rawGammaChannelCount;
    UInt32                      rawGammaDataCount;
    UInt32                      rawGammaDataWidth;

    IOByteCount                 clutDataLen;
    UInt8 *                     clutData;
    UInt32                      clutIndex;
    UInt32                      clutOptions;

    uint32_t                    framebufferWidth;
    uint32_t                    framebufferHeight;
    uint32_t                    consoleDepth;
	uint32_t					restoreType;
	uint32_t					hibernateGfxStatus;
    uint32_t                    saveLength;
    void *                      saveFramebuffer;

	UInt8						needGammaRestore;
	UInt8						vblThrottle;
	UInt8						_reservedB;
    UInt8                       gammaNeedSet;
    UInt8                       gammaScaleChange;
    UInt8                       scaledMode;
    UInt8                       visiblePending;
    UInt8                       testingCursor;
    UInt8                       index;
	UInt8						gammaSet;
    UInt8                       cursorSlept;
    UInt8                       cursorPanning;
    UInt8                       pendingSpeedChange;
    bool                        pendingUsable;

    UInt8                       lli2c;
    UInt8                       cursorClutDependent;
    UInt8                       allowSpeedChanges;
    UInt8                       dimDisable;

    UInt8                       enableScalerUnderscan;
    UInt8                       userSetTransform;
    UInt8                       closed;
    UInt8                       online;
    UInt8                       bClamshellOffline;
	UInt8						displaysOnline;
    UInt8                       lastNotifyOnline;
    UInt8						_reserved;
    UInt8                       dpInterrupts;
    SInt8                       dpSupported;
    UInt8                       dpDongle;
    UInt8                       dpDongleSinkCount;
    UInt8                       dpBusID;
	UInt8						colorModesAllowed;
	UInt8						needsInit;
	UInt8						audioStreaming;
	UInt8                       refreshBootGraphics;
	UInt8                       wakingFromHibernateGfxOn;
	uint32_t					uiScale;

	uint32_t					colorModesSupported;

    UInt64                      transform;
    UInt64                      selectedTransform;
    UInt32                      reducedSpeed;
    IOService *                 temperatureSensor;
    IOI2CBusTiming              defaultI2CTiming;

    uintptr_t                   gammaScale[4];

    IOPixelInformation          pixelInfo;
	IOTimingInformation 		timingInfo;
    IODisplayModeID             offlineMode;
    IODisplayModeID             aliasMode;
    IODisplayModeID             matchedMode;
    IODisplayModeID             setupMode;
    IOIndex                     currentDepth;

    int32_t                     lastProcessedChange;

    uint32_t                    wsaaState;
    IOReturn                    lastWSAAStatus;

    uint32_t                    fAPIState;

    SInt32                      gammaSyncType;
    
    uint32_t                    fBuiltInPanel:1;
    uint32_t                    fNotificationActive:1;
    uint32_t                    fNotificationGroup;

    IODisplayModeID             lastSuccessfulMode;

    OSObject *                  pmSettingNotificationHandle;

    IOByteCount                 hibernateGammaDataLen;
    uint8_t *                   hibernateGammaData;
    uint32_t                    hibernateGammaChannelCount;
    uint32_t                    hibernateGammaDataCount;
    uint32_t                    hibernateGammaDataWidth;

    uint64_t                    hdcpLimitState;

    bool                        fDeferDozeUntilAck;
    bool                        transactionsEnabled;

    // fServerUsesModernAcks is true if RegisterNotificationPort
    //     type >= kServerAckProtocolGraphicsTypesRev.
    bool                        fServerUsesModernAcks;
    uint16_t                    fServerMsgCount;     // Current sent server msg
    integer_t                   fServerMsgIDSentPower;
    integer_t                   fServerMsgIDAckedPower;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define GetShmem(instance)      ((StdFBShmem_t *)(instance->priv))

#define KICK_CURSOR(thread)     thread->interruptOccurred(0, 0, 0)

#define CLEARSEMA(shmem, inst) do {                               \
    if (inst->__private->cursorToDo)                              \
        KICK_CURSOR(inst->__private->cursorThread);               \
    if (shmem)                                                    \
        OSSpinLockUnlock(&shmem->cursorSema);                     \
} while(false)

#define SETSEMA(shmem) do {                                       \
    if ( !(shmem && OSSpinLockTry(&shmem->cursorSema)) )          \
        return;                                                   \
} while(false)

#define TOUCHBOUNDS(one, two) \
        (((one.minx < two.maxx) && (two.minx < one.maxx)) && \
        ((one.miny < two.maxy) && (two.miny < one.maxy)))

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOGraphicsWorkLoop : public IOWorkLoop
{
    OSDeclareDefaultStructors(IOGraphicsWorkLoop)
public:
    typedef IOWorkLoop super;
	typedef void GateFunction(IOWorkLoop * wl, OSObject * obj, void * reference, bool gate);

#if DEADLOCK_DETECT
    static IOLock *sOwnersLock;
    static OSArray *sOwners;
#endif

	IOLock *        gateMutex;
	thread_t		gateThread;
	uint32_t		gateCount;

	IOOptionBits   options;
	GateFunction * func;
	OSObject *     obj;
	void *         reference;

    static IOGraphicsWorkLoop * workLoop(IOGraphicsWorkLoop * me,
                                          IOOptionBits options = 0,
                                          GateFunction * func = NULL,
                                          OSObject * obj = NULL,
                                          void * reference = NULL);
	virtual bool init() APPLE_KEXT_OVERRIDE;
	virtual void free() APPLE_KEXT_OVERRIDE;
    virtual void taggedRelease(const void *tag) const APPLE_KEXT_OVERRIDE;

	virtual void signalWorkAvailable() APPLE_KEXT_OVERRIDE { IOWorkLoop::signalWorkAvailable(); }
    virtual bool inGate() const APPLE_KEXT_OVERRIDE;
    virtual void closeGate() APPLE_KEXT_OVERRIDE;
    virtual void openGate() APPLE_KEXT_OVERRIDE;
    virtual bool tryCloseGate() APPLE_KEXT_OVERRIDE;
    virtual int  sleepGate(void *event, UInt32 interuptibleType) APPLE_KEXT_OVERRIDE;
    virtual int  sleepGate(void *event, AbsoluteTime deadline, UInt32 interuptibleType) APPLE_KEXT_OVERRIDE;
    virtual void wakeupGate(void *event, bool oneThread) APPLE_KEXT_OVERRIDE;

    void timedCloseGate(const char * name, const char * fn)
    {
        IOG_KTRACE_IFSLOW_START(DBG_IOG_TIMELOCK);

        closeGate();

        uint64_t nameBufInt[1] = {0}; GPACKSTRING(nameBufInt, name);
        uint64_t fnBufInt[2]   = {0}; GPACKSTRING(fnBufInt,   fn);
        IOG_KTRACE_IFSLOW_END(DBG_IOG_TIMELOCK, DBG_FUNC_NONE,
                              kGTRACE_ARGUMENT_STRING, nameBufInt[0],
                              kGTRACE_ARGUMENT_STRING, fnBufInt[0],
                              kGTRACE_ARGUMENT_STRING, fnBufInt[1],
                              kTIMELOCK_TIMEOUT_AT);
    }

    //
    // The GateGuard is an alternative and much safer way of controlling the
    // workloops' lock. It uses a design feature of C++ which specifies that a
    // local object will be destructed when the object exits scope. This
    // means that if you declare an IOGraphicsWorkLoop::GateGuard object as a
    // local variable then from the point of declaration until the end of scope
    // the WorkLoop's gate will be closed and it will autmatically be opened at
    // end of scope. With recursive locks, which the gate is based upon,
    // matching locks and unlocks is vital imporant and missmatches lead to
    // very difficult to debug problems. The use of a guard guarantees that for
    // every lock there MUST be an unlock, the compiler enforces it.
    //
    class GateGuard {
    public:
        GateGuard(IOGraphicsWorkLoop *inWl, const char *name, const char *fn)
            : wl(inWl) { wl->timedCloseGate(name, fn); }
        ~GateGuard() { wl->openGate(); }
    private:
        IOGraphicsWorkLoop * const wl;
    };
};

// IOGraphicsSystemWorkLoop, IOGraphicsControllerWorkLoop are subclassed so
// that stacks waiting on a lock are unambiguous about which lock they want.
class IOGraphicsSystemWorkLoop : public IOGraphicsWorkLoop
{
    OSDeclareDefaultStructors(IOGraphicsSystemWorkLoop);
    typedef IOGraphicsWorkLoop super;

public:
    static super * workLoop(IOOptionBits options = 0,
                            GateFunction * func = NULL,
                            OSObject * obj = NULL,
                            void * reference = NULL)
    {
        return super::workLoop(new IOGraphicsSystemWorkLoop, options, func,
                               obj, reference);
    }

    __attribute__ ((disable_tail_calls)) __attribute__ ((noinline))
    virtual void closeGate() APPLE_KEXT_OVERRIDE
    {
        super::closeGate();
    }

    __attribute__ ((disable_tail_calls)) __attribute__ ((noinline))
    virtual int sleepGate(void *event, UInt32 interuptibleType) APPLE_KEXT_OVERRIDE
    {
        return super::sleepGate(event, interuptibleType);
    }

    __attribute__ ((disable_tail_calls)) __attribute__ ((noinline))
    virtual int sleepGate(void *event, AbsoluteTime deadline, UInt32 interuptibleType) APPLE_KEXT_OVERRIDE
    {
        return super::sleepGate(event, deadline, interuptibleType);
    }
};

class IOGraphicsControllerWorkLoop : public IOGraphicsWorkLoop
{
    OSDeclareDefaultStructors(IOGraphicsControllerWorkLoop)
    typedef IOGraphicsWorkLoop super;

public:
    static super * workLoop(IOOptionBits options = 0,
                            GateFunction * func = NULL,
                            OSObject * obj = NULL,
                            void * reference = NULL)
    {
        return super::workLoop(new IOGraphicsControllerWorkLoop, options, func,
                               obj, reference);
    }

    __attribute__ ((disable_tail_calls)) __attribute__ ((noinline))
    virtual void closeGate() APPLE_KEXT_OVERRIDE
    {
        super::closeGate();
    }

    __attribute__ ((disable_tail_calls)) __attribute__ ((noinline))
    virtual int sleepGate(void *event, UInt32 interuptibleType) APPLE_KEXT_OVERRIDE
    {
        return super::sleepGate(event, interuptibleType);
    }

    __attribute__ ((disable_tail_calls)) __attribute__ ((noinline))
    virtual int sleepGate(void *event, AbsoluteTime deadline, UInt32 interuptibleType) APPLE_KEXT_OVERRIDE
    {
        return super::sleepGate(event, deadline, interuptibleType);
    }
};

OSDefineMetaClassAndStructors(IOGraphicsWorkLoop, IOWorkLoop)
OSDefineMetaClassAndStructors(IOGraphicsSystemWorkLoop, IOGraphicsWorkLoop)
OSDefineMetaClassAndStructors(IOGraphicsControllerWorkLoop, IOGraphicsWorkLoop)

#if DEADLOCK_DETECT
IOLock *IOGraphicsWorkLoop::sOwnersLock = NULL;
OSArray *IOGraphicsWorkLoop::sOwners = NULL;
#endif

IOGraphicsWorkLoop * IOGraphicsWorkLoop::workLoop(IOGraphicsWorkLoop * me,
    IOOptionBits options, GateFunction * func, OSObject * obj, void * reference)
{
    if (!me)
		return (NULL);

	me->options   = options;
	me->func      = func;
	me->obj       = obj;
	me->reference = reference;
	if (!me->init())
	{
		me->release();
		me = NULL;
	}

    return (me);
}

bool IOGraphicsWorkLoop::init()
{
	bool ok;

	gateMutex = IOLockAlloc();
    if (!gateMutex)	return (false);

	ok = IOWorkLoop::init();

	if (gateLock)
	{
	    IORecursiveLockFree(gateLock);
	    gateLock = NULL;
	}

#if DEADLOCK_DETECT
    if (!sOwnersLock) {
        IOLock *lock = IOLockAlloc();
        if (lock) {
            while (!sOwnersLock && !OSCompareAndSwapPtr(NULL, lock, &sOwnersLock))
            {
                // Do nothing while trying to install our lock until someone
                // (not necessarily us) succeeds.
            }
            if (sOwnersLock != lock) {
                IOLockFree(lock);
                lock = NULL;
            } else {
                sOwners = OSArray::withCapacity(4);
            }
        }
    }

    if (ok && sOwners) {
        IOLockLock(sOwnersLock);
        sOwners->setObject(this);
        IOLockUnlock(sOwnersLock);
    }
#endif

	return (ok);
}

void IOGraphicsWorkLoop::taggedRelease(const void *tag) const
{
    // If DEADLOCK_DETECT then free() when sOwners is the only retain left
#if DEADLOCK_DETECT
#define TAGGED_RELEASE_FREE_WHEN 2
#else
#define TAGGED_RELEASE_FREE_WHEN 1
#endif
    super::taggedRelease(tag, TAGGED_RELEASE_FREE_WHEN);
}

/* Caution: Tightly coupled to IOWorkLoop::free().
 * This is called twice: once with workThread != NULL but not necessarily
 * running on the work thread, and again from the work thread with
 * workThread == NULL just before the thread terminates. */
void IOGraphicsWorkLoop::free()
{
    IOThread _workThread = workThread;
    IOLock *_gateMutex = gateMutex;

#if DEADLOCK_DETECT
    if (!_workThread && sOwners) {
        IOLockLock(sOwnersLock);
        unsigned i = sOwners->getNextIndexOfObject(this, 0);
        if ((unsigned)-1 != i) {
            sOwners->removeObject(i);
        }
        IOLockUnlock(sOwnersLock);
    }
#endif

    IOWorkLoop::free();

    if (!_workThread) {
        // Caution: ``this'' now points to freed memory!
        if (_gateMutex) {
            IOLockFree(_gateMutex);
            _gateMutex = NULL;
        }
    }
}

bool IOGraphicsWorkLoop::inGate() const
{
    return (gateThread == IOThreadSelf());
}

void IOGraphicsWorkLoop::closeGate()
{
	if (gateThread == IOThreadSelf())
	{
		if (!gateCount) panic("gateCount");
		gateCount++;
	}
	else
	{
#if DEADLOCK_DETECT
        // Bad news if you're taking SYS WL when holding any of the controller WLs.
        // This should uncover it right away instead of waiting for actual deadlocks.
        if (this == gIOFBSystemWorkLoop) {
            IOGraphicsWorkLoop *otherWL = NULL;
            IOLockLock(sOwnersLock);
            for (unsigned i = 0; i < sOwners->getCount(); i++) {
                otherWL = (IOGraphicsWorkLoop *)sOwners->getObject(i);
                if ((otherWL != this) && otherWL->inGate()) {
                    break;
                } else {
                    otherWL = NULL;
                }
            }
            IOLockUnlock(sOwnersLock);

            if (otherWL) {
                OSReportWithBacktrace("IOGraphics: lock order violation\n");
                assert(!"IOGraphics: lock order violation\n");
            }
        }
#endif

#if 0
        const auto startTime = mach_absolute_time();
#endif
        IOLockLock(gateMutex);
#if 0
        const auto deltams = at2ms(mach_absolute_time() - startTime);
        if (deltams >= 50)
            OSReportWithBacktrace("wslow %lld ms\n", deltams);
#endif
        assert (gateThread == 0);
        assert (gateCount == 0);
		if (gateThread) panic("gateThread");
		if (gateCount)  panic("gateCount");

        gateThread = IOThreadSelf();
        gateCount  = 1;
		if (func) (*func)(this, obj, reference, true);
	}
}

void IOGraphicsWorkLoop::openGate()
{
    assert (gateThread == IOThreadSelf());
    if (gateThread != IOThreadSelf()) panic("gateThread");
    if (1 == gateCount)
    {
		if (func) (*func)(this, obj, reference, false);
		if (gateThread != IOThreadSelf()) panic("gateThread");
		if (gateCount != 1)  panic("gateCount");
        gateThread = NULL;
        gateCount = 0;
        IOLockUnlock(gateMutex);
        return;
    }
	gateCount--;
}

bool IOGraphicsWorkLoop::tryCloseGate()
{
	bool gotit = true;

	if (gateThread == IOThreadSelf()) gateCount++;
	else
	{
        if (!IOLockTryLock(gateMutex)) gotit = false;
        else
        {
			assert (gateThread == 0);
			assert (gateCount == 0);
			if (gateThread) panic("gateThread");
			if (gateCount)  panic("gateCount");
			gateThread = IOThreadSelf();
			gateCount  = 1;
			if (func) (*func)(this, obj, reference, true);
		}
	}
	return (gotit);
}

int IOGraphicsWorkLoop::sleepGate(void *event, UInt32 interuptibleType)
{
	int      result;
	uint32_t count;

    assert(gateThread == IOThreadSelf());
    if (gateThread != IOThreadSelf()) panic("gateThread");

	count     = gateCount;
	gateCount = 0;
	if (func) (*func)(this, obj, reference, false);

	gateThread = NULL;
	result = IOLockSleep(gateMutex, event, interuptibleType);
	
	assert (gateThread == 0);
	assert (gateCount == 0);
	if (gateThread) panic("gateThread");
	if (gateCount)  panic("gateCount");
	gateThread = IOThreadSelf();
	gateCount  = count;

	if (func) (*func)(this, obj, reference, true);
    return (result);
}

int IOGraphicsWorkLoop::sleepGate(void *event, AbsoluteTime deadline, UInt32 interuptibleType)
{
	int      result;
	uint32_t count;

    assert(gateThread == IOThreadSelf());

	count     = gateCount;
	gateCount = 0;
	if (func) (*func)(this, obj, reference, false);

	gateThread = NULL;
	result = IOLockSleepDeadline(gateMutex, event, deadline, interuptibleType);
	
	assert (gateThread == 0);
	assert (gateCount == 0);
	gateThread = IOThreadSelf();
	gateCount  = count;

	if (func) (*func)(this, obj, reference, true);

    return (result);
}

#define kOneThreadWakeup (true)
#define kManyThreadWakeup (false)
void IOGraphicsWorkLoop::wakeupGate(void *event, bool oneThread)
{
	return (IOLockWakeup(gateMutex, event, oneThread));
}
#pragma mark -

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define SYSUNLOCK()       \
    gIOFBSystemWorkLoop->openGate()
#define SYSASSERTGATED() do { \
    if (!gIOFBSystemWorkLoop->inGate()) panic(__FUNCTION__ " not sys gated?"); \
} while(false)
#define SYSASSERTNOTGATED() do { \
    if (gIOFBSystemWorkLoop->inGate()) panic(__FUNCTION__ " sys gated?"); \
} while(false)
#define SYSGATEGUARD(guardname) IOGraphicsWorkLoop::GateGuard \
    guardname(gIOFBSystemWorkLoop, "S", __FUNCTION__)


#define FCLOCK(fc)        \
    fc->fWl->closeGate()
#define FCUNLOCK(fc)      \
    fc->fWl->openGate()
#define FCASSERTGATED(fc) do { \
    if (!fc->fWl->inGate()) panic(__FUNCTION__ " not controller gated?"); \
} while(false)
#define FCASSERTNOTGATED(fc) do { \
    if (fc->fWl->inGate()) panic(__FUNCTION__ " controller gated?"); \
} while(false)
#define FCGATEGUARD(guardname, fc) IOGraphicsWorkLoop::GateGuard \
        guardname(fc->fWl, fc->fName, __FUNCTION__)

#define FBWL(fb)          \
    fb->__private->controller->fWl
#define FBUNLOCK(fb)       \
    FBWL(fb)->openGate()
#define FBASSERTGATED(fb) do { \
    if (!FBWL(fb)->inGate()) panic(__FUNCTION__ " not framebuffer gated?"); \
} while(false)
#define FBASSERTNOTGATED(fb) do { \
    if (FBWL(fb)->inGate()) panic(__FUNCTION__ " framebuffer gated?"); \
} while(false)

#define FBLOCK(fb) (FBWL(fb))->timedCloseGate((fb)->thisName, __FUNCTION__)
#define FBGATEGUARD(guardname, fb) IOGraphicsWorkLoop::GateGuard \
        guardname(FBWL(fb), (fb)->thisName, __FUNCTION__)

#define SYSISLOCKED()         gIOFBSystemWorkLoop->inGate()
#define FCISLOCKED(fc)        fc->fWl->inGate()
#define FBISLOCKED(fb)        FBWL(fb)->inGate()

#if TIME_LOGS

#define TIMESTART()                                                            \
{                                                                              \
    const uint64_t _TS_startTime = mach_absolute_time();

#define TIMEEND(name, fmt, args...)								               \
    if (gIOGraphicsDebugCategories & DEBG_CATEGORIES_BUILD & DC_BIT(TIME)) {   \
        const uint64_t _TS_endTime = mach_absolute_time();                     \
        const auto _TS_nowms = static_cast<uint32_t>(at2ms(_TS_endTime));      \
        const uint64_t _TS_deltams = at2ms(_TS_endTime - _TS_startTime);       \
        KPRINTF("%08d [%s]::%s" fmt,                                           \
                _TS_nowms, name, __FUNCTION__, ## args, _TS_deltams);          \
    }                                                                          \
}

#else	/* !TIME_LOGS */

#define TIMESTART()
#define TIMEEND(name, fmt, args...)

#endif	

#define CURSORTAKELOCK(fb)                                              \
    if (!cursorEnable) return;                                          \
    FBGATEGUARD(ctrlgated, fb);                                         \
    shmem = GetShmem(fb);                                               \
    SETSEMA(shmem)

#if TIME_CURSOR

static const uint64_t kCursorThresholdAT = ms2at(20);
#define CURSORLOCK(fb)                                                       \
    StdFBShmem_t *shmem = nullptr;                                           \
    if (2 != fb->getPowerState()) {                                          \
        CURSORTAKELOCK(fb);                                                  \
    } else {                                                                 \
        uint64_t nameBufInt[1] = {0}; GPACKSTRING(nameBufInt, fb->thisName); \
        uint64_t fnBufInt[2]   = {0}; GPACKSTRING(fnBufInt,   __FUNCTION__); \
        IOG_KTRACE_IFSLOW_START(DBG_IOG_CURSORLOCK);                         \
        CURSORTAKELOCK(fb);                                                  \
        IOG_KTRACE_IFSLOW_END(DBG_IOG_CURSORLOCK, DBG_FUNC_NONE,             \
                              kGTRACE_ARGUMENT_STRING, nameBufInt[0],        \
                              kGTRACE_ARGUMENT_STRING, fnBufInt[0],          \
                              kGTRACE_ARGUMENT_STRING, fnBufInt[1],          \
                              kCursorThresholdAT);                           \
    }

#else // !TIME_CURSOR

#define CURSORLOCK(fb) StdFBShmem_t *shmem; CURSORTAKELOCK(fb)

#endif // TIME_CURSOR

#define CURSORUNLOCK(fb)	CLEARSEMA(shmem, fb)


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define extEntry(allowOffline, _apibit_)        _extEntry(false, allowOffline, _apibit_, __FUNCTION__)
#define extExit(result, _apibit_)               _extExit (false, result, _apibit_, __FUNCTION__)
#define extEntrySys(allowOffline, _apibit_)     _extEntry(true,  allowOffline, _apibit_, __FUNCTION__)
#define extExitSys(result, _apibit_)            _extExit (true,  result, _apibit_, __FUNCTION__)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOFramebufferParameterHandler : public IODisplayParameterHandler
{
    OSDeclareDefaultStructors(IOFramebufferParameterHandler)

    OSDictionary *      fDisplayParams;
    IOFramebuffer *     fFramebuffer;
    IODisplay *         fDisplay;

public:
    static IOFramebufferParameterHandler * withFramebuffer( IOFramebuffer * framebuffer );
    virtual void free() APPLE_KEXT_OVERRIDE;

    virtual bool setDisplay( IODisplay * display ) APPLE_KEXT_OVERRIDE;
    virtual bool doIntegerSet( OSDictionary * params,
                               const OSSymbol * paramName, UInt32 value ) APPLE_KEXT_OVERRIDE;
    virtual bool doDataSet( const OSSymbol * paramName, OSData * value ) APPLE_KEXT_OVERRIDE;
    virtual bool doUpdate( void ) APPLE_KEXT_OVERRIDE;

    void displayModeChange( void );
};

class IOFramebufferSensor : public IOService
{
    OSDeclareDefaultStructors(IOFramebufferSensor)

    IOFramebuffer *     fFramebuffer;

public:
    static IOFramebufferSensor * withFramebuffer( IOFramebuffer * framebuffer );
    virtual void free() APPLE_KEXT_OVERRIDE;

    virtual IOReturn callPlatformFunction( const OSSymbol * functionName,
                                           bool waitForFunction,
                                           void *param1, void *param2,
                                           void *param3, void *param4 ) APPLE_KEXT_OVERRIDE;

    virtual IOReturn callPlatformFunction( const char * functionName,
                                           bool waitForFunction,
                                           void *param1, void *param2,
                                           void *param3, void *param4 ) APPLE_KEXT_OVERRIDE;
};
#pragma mark -

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOFramebufferI2CInterface : public IOI2CInterface
{
    OSDeclareDefaultStructors(IOFramebufferI2CInterface)

    IOFramebuffer *     fFramebuffer;
    SInt32              fBusID;
    UInt32              fSupportedTypes;
    UInt32              fSupportedCommFlags;
#if RLOG
    char                fName[32];
#endif
    
public:
    virtual bool start( IOService * provider ) APPLE_KEXT_OVERRIDE;
    virtual IOReturn startIO( IOI2CRequest * request ) APPLE_KEXT_OVERRIDE;

    static IOFramebufferI2CInterface * withFramebuffer( IOFramebuffer * framebuffer, 
                                                        OSDictionary * info );
    static IOReturn create( IOFramebuffer * framebuffer, const char *fbName );

    virtual bool willTerminate(IOService *provider, IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual bool didTerminate(IOService *provider, IOOptionBits options, bool *defer) APPLE_KEXT_OVERRIDE;
    virtual bool requestTerminate(IOService *provider, IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual bool terminate(IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual bool finalize(IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;

};
#pragma mark -

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// IOFBController computed states
enum {
    kIOFBDidWork 			 = 0x00000001,
//    kIOFBWorking               = 0x00000002,
    kIOFBPaging   			 = 0x00000004,
    kIOFBWsWait   			 = 0x00000008,
    kIOFBDimmed   			 = 0x00000010,
    kIOFBServerSentPowerOff  = 0x00000020,	// any fb ws notified asleep
    kIOFBServerAckedPowerOn  = 0x00000040,	// any fb ws state awake
    kIOFBServerAckedPowerOff = 0x00000080,	// any fb ws state asleep
    kIOFBCaptured 			 = 0x00000100,
    kIOFBDimDisable 		 = 0x00000200,
    kIOFBDisplaysChanging 	 = 0x00001000,
};

// IOFBController work states for fDidWork and fAsyncWork
enum {
	kWorkStateChange = 0x00000001,
	kWorkPower       = 0x00000002,
	kWorkSuspend     = 0x00000004,
};

OSDefineMetaClassAndStructors(IOFBController, OSObject)

#define GET_FRAMEBUFFER(a, i) static_cast<IOFramebuffer*>(a->getObject(i))
#define FORALL_FRAMEBUFFERS(fb, a) \
    for (unsigned _i_ = 0; (fb = GET_FRAMEBUFFER(a, _i_)); ++_i_)
#define FOREACH_FRAMEBUFFER(fb) for (int i = 0; (fb = fFbs[i]); i++)

IOFBController *IOFBController::withFramebuffer(IOFramebuffer *fb)
{
    IOFBC_START(withFramebuffer,0,0,0);
    IOFBController *me = new IOFBController;
    if (me && !me->init(fb))
        OSSafeReleaseNULL(me);
    IOFBC_END(withFramebuffer,0,0,0);
    return me;
}

bool IOFBController::init(IOFramebuffer *fb)
{
    IOFBC_START(init,0,0,0);
    SYSASSERTGATED();

    if (!super::init())
    {
        IOFBC_END(init,false,0,0);
        return false;
    }

    OSObject *depPropObj = fb->copyProperty(kIOFBDependentIDKey);
    if (depPropObj && !OSDynamicCast(OSNumber, depPropObj))
        OSSafeReleaseNULL(depPropObj);
    fDependentID = static_cast<OSNumber*>(depPropObj);

    IOService *service = fb;
    while ((service = service->getProvider()))
        if (OSDynamicCast(IOPCIDevice, service))
            break;
    if (service)
    {
        fDevice = service;
        fIntegrated =  (0 == ((IOPCIDevice *)service)->getBusNumber());
        fExternal = (NULL != service->getProperty(kIOPCITunnelledKey, gIOServicePlane,
            kIORegistryIterateRecursively | kIORegistryIterateParents));

        static const char   eGPUName[] = "EGPU";
        static const char   eGPURegistryName[] = "display";
        static uint8_t      eGPUCount = 0;
        if ((0 == strncmp(eGPURegistryName, service->getName(), strlen(eGPURegistryName))) && fExternal)
        {
            fDGPUNameLen = strlen(eGPUName) + 4; // Max 3 digits plus terminator
            fDGPUName = IONew(char, fDGPUNameLen);
            if (NULL != fDGPUName)
            {
                snprintf(fDGPUName, fDGPUNameLen, "%s%u", eGPUName, eGPUCount);
                fName = fDGPUName;
            }
            else
            {
                fName = service->getName();
                fDGPUNameLen = 0;
            }
        }
        else
        {
            fName = service->getName();
        }

        OSData *data = OSDynamicCast(OSData, service->getProperty("vendor-id"));
        if (data)
            fVendorID = ((uint32_t *) data->getBytesNoCopy())[0];

        if ((fVendorID == kPCI_VID_NVIDIA) ||
            (fVendorID == kPCI_VID_NVIDIA_AGEIA)) {
            (void) atomic_fetch_or(&gIOGDebugFlags, kIOGDbgNoClamshellOffline);
            kprintf("IOG CS_OFF: %#llx\n", gIOGDebugFlags);
        }
    }
    else
        fName = "FB??";

#if SINGLE_THREAD
    fWl = gIOFBSystemWorkLoop;
#else
    fWl = IOGraphicsControllerWorkLoop::workLoop(0, NULL, NULL, this);
#endif
    if (!fWl)
        panic("controller->fWl");
    IOInterruptEventSource::Action action = OSMemberFunctionCast(
            IOInterruptEventSource::Action, this, &IOFBController::asyncWork);
    fWorkES = IOInterruptEventSource::interruptEventSource(this, action);
    if (!fWorkES)
        panic("controller->fWorkES");
    fWl->addEventSource(fWorkES);

    setState(kIOFBNotOpened);

    STAILQ_INSERT_TAIL(&gIOFBAllControllers, this, fNextController);

    IOFBC_END(init,true,0,0);
    return true;
}

void IOFBController::unhookFB(IOFramebuffer *fb)
{
    IOFBC_START(unhookFB,0,0,0);
    SYSASSERTGATED();

    if (fWorkES) {
        fWl->removeEventSource(fWorkES);
        OSSafeReleaseNULL(fWorkES);

        // Must be on gIOFBAllControllers STAILQ if fWorkES exists
        STAILQ_REMOVE(&gIOFBAllControllers,
                      this, IOFBController, fNextController);
    }

    for (uint32_t i = 0; i < fMaxFB; i++) {
        if (fb == fFbs[i]) fFbs[i] = NULL;
    }
    IOFBC_END(unhookFB,0,0,0);
}

void IOFBController::free()
{
    IOFBC_START(free,0,0,0);
    OSSafeReleaseNULL(fDependentID);
    if (fWorkES) {
        fWl->removeEventSource(fWorkES);
        OSSafeReleaseNULL(fWorkES);

        // Must be on gIOFBAllControllers STAILQ if fWorkES exists
        SYSGATEGUARD(sysgated);
        STAILQ_REMOVE(&gIOFBAllControllers,
                      this, IOFBController, fNextController);
    }
    OSSafeReleaseNULL(fWl);

    if (fDGPUName)
    {
        IODelete(fDGPUName, char, fDGPUNameLen);
        fDGPUName = NULL;
    }

    IOFBC_END(free,0,0,0);
}

/*! @function copyController
    @abstract Lookup or create an IOFBController depending on creation flag.
    @discussion Framebuffer Controllers are kept in a linked list and framebuffers use copyController to rendezvous with their underlying controller. The linked list is protected by the system workloop.
    @param fb The Framebuffer that wants to find/create its controller.
    @param flag <IOFBController::CopyType>. kLookupOnly and kDepIDCreate will lookup a controller indexed by the FB Property kIOFBDependentIDKey. The DepIDCreate flag will create a controller given the property but will return NULL if the property is not set. kForceCreate will always create a controller with the current DependentID or NULL if it is not set yet. DepIDCreate is used by IOFramebuffer::start() and IOFramebuffer::open() uses ForceCreate and Lookup.
    @result An retained IOFBController or NULL if one could not be found or created.
*/
IOFBController *IOFBController::copyController(IOFramebuffer *fb, const CopyType flag)
{
    IOFBC_START(copyController,flag,0,0);
    SYSASSERTGATED();

    // Must have a valid dependent ID when doing a Lookup or DepIDCreate.
    OSObject *depID = fb->getProperty(kIOFBDependentIDKey);
    if ((kDepIDCreate == flag || kLookupOnly == flag)
    && !OSDynamicCast(OSNumber, depID))
    {
        IOFBC_END(copyController,-1,0,0);
        return NULL;
    }

    // TODO(gvdl): Should all FBs without dependent ID use a single controller?
    // Until I understand the dynamics better I'll keep the previous semantic
    // of create a new controller for every such framebuffer. This loop will
    // only search for controllers with dependent IDs.
    IOFBController *ret = NULL;
    STAILQ_FOREACH(ret, &gIOFBAllControllers, fNextController)
        if (ret->fDependentID && ret->fDependentID->isEqualTo(depID))
            break;

    if (ret)
        ret->retain();
    else if (kLookupOnly != flag)
        ret = IOFBController::withFramebuffer(fb);

    IOFBC_END(copyController,0,0,0);
    return ret;
}

IOFBController *IOFBController::alias(const char * fn)
{
    IOFBC_START(alias,0,0,0);
    SYSASSERTGATED();

    if (0 == fAliasID) {
        panic("%s: invalid alias!\n", fn);
    }

    IOFBController *ret;
    STAILQ_FOREACH(ret, &gIOFBAllControllers, fNextController)
        if ((this != ret) && (ret->fAliasID == fAliasID))
            break;

    if ((NULL == ret) && (fAliasID)) {
        IOLog("IOG::%s WARNING: Unable to find alias.\n", fn );
    }

    IOFBC_END(alias,0,0,0);
    return ret;
}

uint32_t IOFBController::computeState()
{
    IOFBC_START(computeState,0,0,0);
    SYSASSERTGATED();

    IOFramebuffer * fb;
    uint32_t state;

    state = fComputedState;

    if (fDidWork || fConnectChange)
    {
        FCGATEGUARD(ctrlgated, this);

        if (fConnectChange)
            fNeedsWork = true;

        if (fDidWork & ~kWorkStateChange)
        {
            fAsyncWork = 0;

            FOREACH_FRAMEBUFFER(fb)
            {
                if (kWorkPower & fDidWork)
                {
                    if (fb->pendingPowerState)
                    {
                        fb->deliverFramebufferNotification( kIOFBNotifyDidWake,    (void *) true);
                        fb->deliverFramebufferNotification( kIOFBNotifyDidPowerOn, (void *) false);
//                      fb->deliverFramebufferNotification( kIOFBNotifyDidWake,    (void *) false);
                    }
                    else
                        fb->deliverFramebufferNotification( kIOFBNotifyDidPowerOff, (void *) false);
                }
                if (kWorkSuspend & fDidWork)
                {
                    fb->suspend(false);
                }
            }

            if (kWorkSuspend & fDidWork)
            {
                assert(fAliasID); // should only be on muxed controllers

                if (fOnlineMask) 				// bgOn
                {
                    IOFBController *oldController = alias(__FUNCTION__);
                    if (oldController)
                    {
                        FCGATEGUARD(oldctrlgated, oldController);
                        if (oldController->fLastFinishedChange
                                == oldController->fConnectChange)
                        {
                            DEBG1(fName, " mute exit\n");
                        }
                    }
                    messageConnectionChange();
                }
            }
        } // fDidWork & ~kWorkStateChange

        state = 0;
        if (fWsWait)
            state |= kIOFBWsWait;

        FOREACH_FRAMEBUFFER(fb)
        {
            // Alias for readability
            const auto& sentPower  = fb->__private->fServerMsgIDSentPower;
            const auto& ackedPower = fb->__private->fServerMsgIDAckedPower;

            if (fb->pagingState)
                state |= kIOFBPaging;
            if (fb->messaged)
                state |= kIOFBDisplaysChanging;
            if (!fb->getIsUsable())
                state |= kIOFBDimmed;
            if ((sentPower  & kIOFBNS_MessageMask) != kIOFBNS_Wake)
                state |= kIOFBServerSentPowerOff;
            if ((ackedPower & kIOFBNS_MessageMask) == kIOFBNS_Wake)
                state |= kIOFBServerAckedPowerOn;
            else
                state |= kIOFBServerAckedPowerOff;
            if (fb->captured)
                state |= kIOFBCaptured;
            if (fb->getDimDisable())
                state |= kIOFBDimDisable;
        }

        fComputedState = state;
        fDidWork = 0;
        state |= kIOFBDidWork;
    }

    DEBG1(fName, " state %x\n", state);

    IOFBC_END(computeState,state,0,0);
    return (state);
}

void IOFBController::startAsync(uint32_t asyncWork)
{
    IOFBC_START(startAsync,asyncWork,0,0);
    SYSASSERTGATED();
    FCASSERTGATED(this);

    if (fAsyncWork)                     panic("fAsyncWork");
    if (fDidWork)                       panic("fDidWork");

    fAsyncWork = asyncWork;
    fWorkES->interruptOccurred(0, 0, 0);
    IOFBC_END(startAsync,0,0,0);
}

void IOFBController::startThread()
{
    IOFBC_START(startThread,0,0,0);
    FCASSERTGATED(this);

    fNeedsWork = true;

    fDidWork |= kWorkStateChange;

    IOFramebuffer::startThread(false);
    IOFBC_END(startThread,0,0,0);
}

void IOFBController::didWork(IOOptionBits work)
{
    IOFBC_START(didWork,work,0,0);
    FCASSERTGATED(this);

    if (fAsyncWork && !work) OSReportWithBacktrace("asyncWork+did");

    fDidWork |= work;

    IOFramebuffer::startThread(false);
    IOFBC_END(didWork,0,0,0);
}

void IOFBController::asyncWork(IOInterruptEventSource * evtSrc, int intCount)
{
    IOFBC_START(asyncWork,intCount,0,0);
    FCASSERTGATED(this);

    uint32_t work = fAsyncWork;
    IOFramebuffer *fb;

    fAsyncWork = kWorkStateChange;

    DEBG1(fName, " (%d)\n", work);
    IOG_KTRACE_NT(DBG_IOG_ASYNC_WORK, DBG_FUNC_START,
        fFbs[0]->__private->regID, work, fDidWork, 0);

    if (kWorkPower & work)
    {
        uint32_t newState = fFbs[0]->pendingPowerState;
        DEBG1(fName, " async kIODriverPowerAttribute(%d, %d)\n",
              newState, fFbs[0]->pendingPowerState);
        FOREACH_FRAMEBUFFER(fb)
        {
            FB_START(setAttribute,kIODriverPowerAttribute,__LINE__,newState);
            fb->setAttribute(kIODriverPowerAttribute, newState);
            FB_END(setAttribute,0,__LINE__,0);
        }
        didWork(kWorkPower);
    }
    else if (kWorkSuspend & work)
    {
        const IOSelect timingDisable = kConnectionPanelTimingDisable;
        FOREACH_FRAMEBUFFER(fb)
        {
            FB_START(setAttributeForConnection,timingDisable,__LINE__,false);
            fb->setAttributeForConnection(0, timingDisable, false);
            FB_END(setAttributeForConnection,0,__LINE__,0);
        }

        FOREACH_FRAMEBUFFER(fb)
        {
            fb->processConnectChange(bg);
        }

        if (fOnlineMask)
        {
            FOREACH_FRAMEBUFFER(fb)
            {
                if (fb->__private->online)
                    fb->matchFramebuffer();
            }
        }
        else
        {
            fLastFinishedChange = fConnectChange;
        }

        FOREACH_FRAMEBUFFER(fb)
        {
            FB_START(setAttributeForConnection,timingDisable,__LINE__,true);
            fb->setAttributeForConnection(0, timingDisable, true);
            FB_END(setAttributeForConnection,0,__LINE__,0);
        }

        didWork(kWorkSuspend);
    }

    IOG_KTRACE_NT(DBG_IOG_ASYNC_WORK, DBG_FUNC_END,
        fFbs[0]->__private->regID, fAsyncWork, fDidWork, 0);
    IOFBC_END(asyncWork,0,0,0);
}

IOOptionBits IOFBController::checkPowerWork(IOOptionBits state)
{
    IOFBC_START(checkPowerWork,state,0,0);
    FCASSERTGATED(this);

    IOFramebuffer *fb;

    if (fPendingMuxPowerChange)
    {
        uint32_t newState = fFbs[0]->pendingPowerState;
        DEBG1(fName, " async checkPowerWork(%d)\n", newState);
        if (!newState)
        {
            FOREACH_FRAMEBUFFER(fb)
            {
//				fb->deliverFramebufferNotification(kIOFBNotifyWillSleep,    (void *) false);
                fb->deliverFramebufferNotification(kIOFBNotifyWillPowerOff, (void *) false);
                fb->deliverFramebufferNotification(kIOFBNotifyWillSleep,    (void *) true);
            }
        }
        fPendingMuxPowerChange = false;
        startAsync(kWorkPower);
    }
    else FOREACH_FRAMEBUFFER(fb)
    {
        state |= fb->checkPowerWork(state);
    }

    IOFBC_END(checkPowerWork,state,0,0);
    return (state);
}

IOOptionBits IOFBController::checkConnectionWork(IOOptionBits state)
{
    IOFBC_START(checkConnectionWork,state,0,0);
    FCASSERTGATED(this);

    IOFramebuffer * fb;

    DEBG1(fName, " count(%d, msg %d, fin %d, proc %d, wake %d),"
                 " capt(%d) fWsWait(%d)\n",
          fConnectChange, fLastMessagedChange, fLastFinishedChange,
          fFbs[0]->__private->lastProcessedChange, fPostWakeChange,
          (0 != (kIOFBCaptured & state)), fWsWait);

    if (kIOFBCaptured & state)
    {
        if (fConnectChange != fLastForceRetrain)
        {
            fLastForceRetrain = fConnectChange;
            FOREACH_FRAMEBUFFER(fb)
            {
                if (!fb->__private->dpSupported)
                {
                    IOReturn err;
                    uintptr_t value[16];
                    FB_START(getAttributeForConnection,kConnectionHandleDisplayPortEvent,__LINE__,0);
                    err = fb->getAttributeForConnection(0, kConnectionHandleDisplayPortEvent, &value[0]);
                    FB_END(getAttributeForConnection,err,__LINE__,0);
                    fb->__private->dpSupported = (kIOReturnSuccess == err) ? true : -1;
                    DEBG1(fb->thisName, " dpSupported(%d)\n", fb->__private->dpSupported);
                }

                if (fb->__private->dpSupported > 0)
                {
                    IOReturn err;
                    uintptr_t sel = kIODPEventForceRetrain;
                    FB_START(setAttributeForConnection,kConnectionHandleDisplayPortEvent,__LINE__,0);
                    err = fb->setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);
                    FB_END(setAttributeForConnection,err,__LINE__,0);
                    DEBG1(fb->thisName, " kIODPEventForceRetrain(0x%x)\n", err);
                }
            }

            const auto c1 =
                GPACKUINT16T(0, fConnectChange) |
                GPACKUINT16T(1, fLastForceRetrain) |
                0;
            const auto c2 =
                GPACKUINT32T(0, fOnlineMask) |
                GPACKBIT(63, isMuted()) |
                0;
            IOG_KTRACE_NT(DBG_IOG_CAPTURED_RETRAIN, DBG_FUNC_NONE,
                fFbs[0]->__private->regID, c1, c2, 0);
        }
        IOFBC_END(checkConnectionWork,state,__LINE__,0);
        return (state);
    }

    if (fMuxNeedsBgOn || fMuxNeedsBgOff)
    {
        if (fMuxNeedsBgOn)
        {
            IOFBController *oldController = alias(__FUNCTION__);
            if (oldController)
            {
                DEBG1(fName, " copy from %s\n", oldController->fName);
            
                FCGATEGUARD(oldctrlgated, oldController);
            
                FOREACH_FRAMEBUFFER(fb)
                {
                    IOFramebuffer *oldfb = oldController->fFbs[i];
                    if (!oldfb || !fb->copyDisplayConfig(oldfb))
                        break;
                }
            }
        }

        const auto c1 =
            GPACKUINT16T(0, fConnectChange) |
            GPACKUINT16T(1, fFbs[0]->__private->lastProcessedChange) |
            GPACKUINT16T(2, fLastFinishedChange) |
            GPACKUINT16T(3, fPostWakeChange);
        const auto c2 =
            GPACKBIT(63, isMuted()) |
            GPACKBIT(62, 1) |
            GPACKBIT(61, fMuxNeedsBgOn) |
            GPACKBIT(60, fMuxNeedsBgOff);
        IOG_KTRACE_NT(DBG_IOG_CONNECT_WORK_ASYNC, DBG_FUNC_NONE,
            fFbs[0]->__private->regID, c1, c2, 0);

        FOREACH_FRAMEBUFFER(fb)
        {
            fb->suspend(true);
        }

        fMuxNeedsBgOn = fMuxNeedsBgOff = false;
        startAsync(kWorkSuspend);
        state |= kIOFBDisplaysChanging;
    }

    if (!fAsyncWork)
        messageConnectionChange();

    IOFBC_END(checkConnectionWork,state,0,0);
    return (state);
}

void IOFBController::messageConnectionChange()
{
    IOFBC_START(messageConnectionChange,0,0,0);
    FCASSERTGATED(this);

    IOReturn 		err;
    bool     		discard = false;
    IOFramebuffer * fb;

    const auto lastMessagedChange = fLastMessagedChange;
    const auto connectChange = fConnectChange;
    const auto changePending = gIOFBIsMuxSwitching
        ? fConnectChangeForMux
        : lastMessagedChange != connectChange;
    const auto messageInProgress = fFbs[0]->messaged;
    if (changePending && !messageInProgress && !fWsWait) {
        IOG_KTRACE_LOG_SYNCH(DBG_IOG_LOG_SYNCH);

        fLastMessagedChange = connectChange;
        fConnectChangeForMux = false;

        if (gIOGraphicsControl)
        {
            err = gIOGraphicsControl->message(kIOFBMessageConnectChange,
                fFbs[0], NULL);
            if (kIOReturnOffline == err)
            {
                DEBG1(fName, " AGC discard\n");
                discard = true;
                fLastFinishedChange = fLastMessagedChange;
                FOREACH_FRAMEBUFFER(fb)
                {
                    fb->__private->lastProcessedChange = fLastMessagedChange;
                }
            }
        }

        if (!discard) {
            uint64_t args[2];
            args[0] = fFbs[0]->__private->regID;
            args[1] = connectChange;
            DEBG1(fName, " messaged RegID:%#llx CC:%#llx\n", args[0], args[1]);
            fFbs[0]->messaged = true;
            fFbs[0]->messageClients(kIOMessageServiceIsSuspended, args,
                sizeof(args));
        }

        const auto c1 =
            GPACKUINT16T(0, fLastFinishedChange) |
            GPACKUINT16T(1, fLastMessagedChange) |
            GPACKUINT16T(2, lastMessagedChange) |
            GPACKUINT16T(3, connectChange);
        const auto c2 =
            GPACKBIT(0, discard) |
            GPACKBIT(1, gIOFBIsMuxSwitching) |
            0;
        IOG_KTRACE_NT(DBG_IOG_MSG_CONNECT_CHANGE, DBG_FUNC_NONE,
            fFbs[0]->__private->regID, c1, c2, 0);
    }

    IOFBC_END(messageConnectionChange,0,0,0);
}
#pragma mark -


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOGraphicsDevice

OSDefineMetaClassAndAbstractStructors(IOFramebuffer, IOGraphicsDevice);

static void IOGraphicsDelayKnob(const char * const name)
{
    static const uint32_t MAX_MS = 60000;
    uint32_t delayMs = 0;
    char bootArgName[64];
    const char *fmt = "IOGraphics %s delay %s (%u ms)\n";
    snprintf(bootArgName, sizeof(bootArgName), "iogdelay_%s", name);
    if (PE_parse_boot_argn(bootArgName, &delayMs, sizeof(delayMs)) && delayMs) {
        if (delayMs > MAX_MS) {
            delayMs = MAX_MS;
        }
        kprintf(fmt, "will", name, delayMs);
        IOSleep(delayMs);
        kprintf(fmt, "did", name, delayMs);
    }
}

static void setupGTraceBuffers()
{
#if GTRACE_REVISION >= 0x1
    if (static_cast<bool>(sAGDCGTrace))
        return;

    if (gIOGATLines)
    {
        gGTrace = GTraceBuffer::make(
                "iogdecoder", "IOGraphicsFamily", gIOGATLines,
                &IOFramebuffer::gTraceData, NULL);
    }
    // Always create an AGDC gtrace buffer.
    sAGDCGTrace = GTraceBuffer::make(
            "agdcdecoder", "AppleGraphicsControl", kGTraceMinimumLineCount,
            &IOFramebuffer::agdcTraceData, NULL);
    // TODO(gvdl): Implement a ModuleStop routine to clear up on unload
    // GTraceBuffer::destroy(iog::move(sAGDCGTrace));
    // GTraceBuffer::destroy(iog::move(gGTrace));
#endif
}

__private_extern__ "C" kern_return_t IOGraphicsFamilyModuleStart(kmod_info_t *ki, void *data);
__private_extern__ "C" kern_return_t IOGraphicsFamilyModuleStart(kmod_info_t *ki, void *data)
{
    IOGraphicsDelayKnob("modstart");

    gIOFBSystemWorkLoop = IOGraphicsSystemWorkLoop::workLoop(0, NULL, NULL, NULL);
    if (!gIOFBSystemWorkLoop) panic("gIOFBSystemWorkLoop");
    gAllFramebuffers     = OSArray::withCapacity(8);
    gStartedFramebuffers = OSArray::withCapacity(1);
    gIOFramebufferKey    = OSSymbol::withCStringNoCopy("IOFramebuffer");

#if DEBG_CATEGORIES_BUILD
    PE_parse_boot_argn("iogdebg", &gIOGraphicsDebugCategories,
                       sizeof(gIOGraphicsDebugCategories));
    sysctl_register_oid(&sysctl__debug_iogdebg);
    D(GENERAL, "IOG", " iogdebg=%#llx builtin=%#llx\n",
      gIOGraphicsDebugCategories, static_cast<uint64_t>(DEBG_CATEGORIES_BUILD));

    sysctl_register_oid(&sysctl__debug_iogdebugflags);
#endif

    // Single threaded at this point no need to be atomically careful
    gIOGDebugFlags = kIOGDbgVBLThrottle | kIOGDbgLidOpen;
    // As part of the static bitmap investigation done during Lobo
    // <rdar://problem/33468295> Black flash / screen dimmed between static bitmap and live content when waking from standby
    // Was root caused to the fadeTimer running = disabled via removal of kIOGDbgFades
    // if (version_major >= 14) gIOGDebugFlags |= kIOGDbgFades;

    uint32_t flags;
    if (PE_parse_boot_argn("iog",  &flags, sizeof(flags))) gIOGDebugFlags |= flags;
    if (PE_parse_boot_argn("niog", &flags, sizeof(flags))) gIOGDebugFlags &= ~flags;

    gIOGATLines = kGTraceDefaultLineCount;
    gIOGATFlags = (kGTRACE_IODISPLAYWRANGLER | kGTRACE_IOFRAMEBUFFER | kGTRACE_FRAMEBUFFER);
    if (PE_parse_boot_argn("iogt", &flags, sizeof(flags)))
        gIOGATFlags |=  flags;
    if (PE_parse_boot_argn("niogt", &flags, sizeof(flags)))
        gIOGATFlags &= ~flags;
    if (!gIOGATFlags)
        gIOGATLines = 0;
    else if (PE_parse_boot_argn("iogtl", &flags, sizeof(flags)))
        gIOGATLines = flags;

    setupGTraceBuffers();

    IOLog("IOG flags 0x%llx (0x%x)\n", gIOGDebugFlags, gIOGATFlags);

    gIOFBLidOpenMode = (0 != (kIOGDbgLidOpen     & gIOGDebugFlags));
    gIOFBVBLThrottle = (0 != (kIOGDbgVBLThrottle & gIOGDebugFlags));
    gIOFBVBLDrift    = (0 != (kIOGDbgVBLDrift    & gIOGDebugFlags));
    gIOGFades        = (0 != (kIOGDbgFades       & gIOGDebugFlags));

    if (!PE_parse_boot_argn("iognotifyto", &gIOGNotifyTO, sizeof(gIOGNotifyTO))
        || !gIOGNotifyTO)
    {
        gIOGNotifyTO = kSystemWillSleepTimeout;
    }
    DEBG1("IOG", " notify timeout %ds\n", gIOGNotifyTO);


    return (kIOReturnSuccess);
}

static bool
IOFramebufferLockedSerialize(void * target, void * ref, OSSerialize * s)
{
    SYSGATEGUARD(sysgated);
    return ((OSObject *) target)->serialize(s);
}                                     

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOFramebuffer::fbLock( void )
{
    IOFB_START(fbLock,0,0,0);
	FBLOCK(this);
    IOFB_END(fbLock,0,0,0);
}

void IOFramebuffer::fbUnlock( void )
{
    IOFB_START(fbUnlock,0,0,0);
    FBUNLOCK(this);
    IOFB_END(fbUnlock,0,0,0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Cursor rendering
 */

#include "IOCursorBlits.h"

inline void IOFramebuffer::StdFBDisplayCursor( IOFramebuffer * inst )
{
    IOFB_START(StdFBDisplayCursor,0,0,0);
    StdFBShmem_t * userShMemPtr;
    StdFBShmem_t sShMem = {0};
    StdFBShmem_t * shmem = &sShMem;
    IOGBounds saveRect;
    volatile unsigned char *vramPtr;    /* screen data pointer */
    unsigned int cursStart;
    unsigned int cursorWidth;
    int width;
    int height;

    userShMemPtr = GetShmem(inst);
    memcpy(shmem, userShMemPtr, sizeof(sShMem));

    saveRect = shmem->cursorRect;
    /* Clip saveRect vertical within screen bounds */
    if (saveRect.miny < shmem->screenBounds.miny)
        saveRect.miny = shmem->screenBounds.miny;
    if (saveRect.maxy > shmem->screenBounds.maxy)
        saveRect.maxy = shmem->screenBounds.maxy;
    if (saveRect.minx < shmem->screenBounds.minx)
        saveRect.minx = shmem->screenBounds.minx;
    if (saveRect.maxx > shmem->screenBounds.maxx)
        saveRect.maxx = shmem->screenBounds.maxx;
    shmem->saveRect = saveRect; /* Remember save rect for RemoveCursor */
    userShMemPtr->saveRect = saveRect; /* Remember save rect for RemoveCursor */

    // <rdar://problem/30217326> CD-IOP Regression - J45G - Launching Pixelmator/Chess causes a hang
    // <rdar://problem/29671853> CD-IOP Regression - J45G - Most Space - repeatedly checking/unchecking AGS causes systems to sometimes hang
    IOMemoryDescriptor * fbRange = inst->getApertureRangeWithLength(kIOFBSystemAperture, inst->rowBytes * inst->__private->framebufferHeight);
    if (NULL != fbRange)
    {
        IOMemoryMap * newVramMap = fbRange->map(kIOFBMapCacheMode);
        fbRange->release();
        if (NULL != newVramMap)
        {
            vramPtr = (volatile unsigned char *)newVramMap->getVirtualAddress();
            if (NULL != vramPtr)
            {
                vramPtr = vramPtr +
                (inst->rowBytes * (saveRect.miny - shmem->screenBounds.miny)) +
                (inst->bytesPerPixel * (saveRect.minx - shmem->screenBounds.minx));

                width = saveRect.maxx - saveRect.minx;
                height = saveRect.maxy - saveRect.miny;
                cursorWidth = shmem->cursorSize[0 != shmem->frame].width;

                cursStart = (saveRect.miny - shmem->cursorRect.miny) * cursorWidth +
                (saveRect.minx - shmem->cursorRect.minx);

                if (inst->cursorBlitProc)
                    inst->cursorBlitProc( inst,
                                         (void *) shmem,
                                         vramPtr,
                                         cursStart,
                                         inst->fTotalWidth - width,   /* vramRow */
                                         cursorWidth - width,      /* cursRow */
                                         width,
                                         height);
            }
            newVramMap->release();
        }
    }
    IOFB_END(StdFBDisplayCursor,0,0,0);
}

// Description: RemoveCursor erases the cursor by replacing the background
//              image that was saved by the previous call to DisplayCursor.
//              If the frame buffer is cacheable, flush at the end of the
//              drawing operation.

inline void IOFramebuffer::StdFBRemoveCursor( IOFramebuffer * inst )
{
    IOFB_START(StdFBRemoveCursor,0,0,0);
    StdFBShmem_t * userShMemPtr;
    StdFBShmem_t sShMem = {0};
    StdFBShmem_t * shmem = &sShMem;
    volatile unsigned char *vramPtr;    /* screen data pointer */
    unsigned int vramRow;
    int width;
    int height;

    userShMemPtr = GetShmem(inst);
    memcpy(shmem, userShMemPtr, sizeof(sShMem));

    /* Clip saveRect vertical within screen bounds */
    if ((shmem->saveRect.miny < shmem->screenBounds.miny)
     || (shmem->saveRect.maxy > shmem->screenBounds.maxy)
     || (shmem->saveRect.minx < shmem->screenBounds.minx)
     || (shmem->saveRect.maxx > shmem->screenBounds.maxx))
	{
		IOLog("%s: bad crsr saverect (%d, %d), (%d, %d) - (%d, %d), (%d, %d)\n",
					inst->thisName,
					shmem->saveRect.minx, shmem->saveRect.miny,
					shmem->saveRect.maxx, shmem->saveRect.maxy,
					shmem->screenBounds.minx, shmem->screenBounds.miny,
					shmem->screenBounds.maxx, shmem->screenBounds.maxy);
        IOFB_END(StdFBRemoveCursor,-1,0,0);
		return;
	}

    vramRow = inst->fTotalWidth; /* Scanline width in pixels */

    // <rdar://problem/30217326> CD-IOP Regression - J45G - Launching Pixelmator/Chess causes a hang
    // <rdar://problem/29671853> CD-IOP Regression - J45G - Most Space - repeatedly checking/unchecking AGS causes systems to sometimes hang
    IOMemoryDescriptor * fbRange = inst->getApertureRangeWithLength(kIOFBSystemAperture, inst->rowBytes * inst->__private->framebufferHeight);
    if (NULL != fbRange)
    {
        IOMemoryMap * newVramMap = fbRange->map(kIOFBMapCacheMode);
        fbRange->release();
        if (NULL != newVramMap)
        {
            vramPtr = (volatile unsigned char *)newVramMap->getVirtualAddress();
            if (NULL != vramPtr)
            {
                vramPtr = vramPtr +
                (inst->rowBytes * (shmem->saveRect.miny - shmem->screenBounds.miny))
                + (inst->bytesPerPixel *
                   (shmem->saveRect.minx - shmem->screenBounds.minx));

                width = shmem->saveRect.maxx - shmem->saveRect.minx;
                height = shmem->saveRect.maxy - shmem->saveRect.miny;
                vramRow -= width;

                if (inst->cursorRemoveProc)
                    inst->cursorRemoveProc( inst, (void *)shmem,
                                           vramPtr, vramRow, width, height);
            }
            newVramMap->release();
        }
    }
    IOFB_END(StdFBRemoveCursor,0,0,0);
}

inline void IOFramebuffer::RemoveCursor( IOFramebuffer * inst )
{
    IOFB_START(RemoveCursor,0,0,0);
    StdFBShmem_t *      shmem = GetShmem(inst);

    if (!inst->pagingState)
    {
        IOFB_END(RemoveCursor,-1,0,0);
        return;
    }

    if (shmem->hardwareCursorActive)
    {
        IOGPoint *              hs;

        hs = &shmem->hotSpot[0 != shmem->frame];
        inst->_setCursorState(
            shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx,
            shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny, false );
    }
    else
        StdFBRemoveCursor(inst);
    IOFB_END(RemoveCursor,0,0,0);
}

inline void IOFramebuffer::DisplayCursor( IOFramebuffer * inst )
{
    IOFB_START(DisplayCursor,0,0,0);
    IOGPoint     *      hs;
    StdFBShmem_t *      shmem = GetShmem(inst);
    SInt32              x, y;

    if (!inst->pagingState)
    {
        IOFB_END(DisplayCursor,-1,0,0);
        return;
    }

    hs = &shmem->hotSpot[0 != shmem->frame];
    x  = shmem->cursorLoc.x - hs->x;
    y  = shmem->cursorLoc.y - hs->y;

    if (shmem->hardwareCursorActive)
        inst->_setCursorState( x - shmem->screenBounds.minx,
                               y - shmem->screenBounds.miny, true );
    else
    {
        shmem->cursorRect.maxx = (shmem->cursorRect.minx = x)
                                 + shmem->cursorSize[0 != shmem->frame].width;
        shmem->cursorRect.maxy = (shmem->cursorRect.miny = y)
                                 + shmem->cursorSize[0 != shmem->frame].height;
        StdFBDisplayCursor(inst);
        shmem->oldCursorRect = shmem->cursorRect;
    }
    IOFB_END(DisplayCursor,0,0,0);
}

inline void IOFramebuffer::SysHideCursor( IOFramebuffer * inst )
{
    IOFB_START(SysHideCursor,0,0,0);
    if (!GetShmem(inst)->cursorShow++)
        RemoveCursor(inst);
    IOFB_END(SysHideCursor,0,0,0);
}

inline void IOFramebuffer::SysShowCursor( IOFramebuffer * inst )
{
    IOFB_START(SysShowCursor,0,0,0);
    StdFBShmem_t *shmem;

    shmem = GetShmem(inst);

    if (shmem->cursorShow)
        if (!--(shmem->cursorShow))
            DisplayCursor(inst);
    IOFB_END(SysShowCursor,0,0,0);
}

inline void IOFramebuffer::CheckShield( IOFramebuffer * inst )
{
    IOFB_START(CheckShield,0,0,0);
    IOGPoint *          hs;
    int                 intersect;
    IOGBounds           tempRect;
    StdFBShmem_t *      shmem = GetShmem(inst);

    /* Calculate temp cursorRect */
    hs = &shmem->hotSpot[0 != shmem->frame];
    tempRect.maxx = (tempRect.minx = (shmem->cursorLoc).x - hs->x)
                    + shmem->cursorSize[0 != shmem->frame].width;
    tempRect.maxy = (tempRect.miny = (shmem->cursorLoc).y - hs->y)
                    + shmem->cursorSize[0 != shmem->frame].height;

    intersect = TOUCHBOUNDS(tempRect, shmem->shieldRect);
    if (intersect != shmem->shielded)
        (shmem->shielded = intersect) ?
        SysHideCursor(inst) : SysShowCursor(inst);
    IOFB_END(CheckShield,0,0,0);
}

#include "AppleLogo.h"
#include "AppleLogo2X.h"

extern "C" size_t lzvn_decode(void * __restrict dst, size_t dst_size, const void * __restrict src, size_t src_size);

#define rgb108(x) (((x) << 2) | ((x) >> 6))

static void IOFramebufferBootInitFB(IOVirtualAddress frameBuffer, 
			uint32_t framebufferWidth, uint32_t framebufferHeight, 
			uint32_t totalWidth, uint32_t consoleDepth, uint8_t logo)
{
	const uint8_t  * src;
	const uint8_t  * clut;
	uint8_t  *       logoData;
	size_t           unpackedSize, srcSize, clutSize;
	uint32_t *       out;
	uint32_t         gray, data, x, y, ox, oy, sx, lw, lh;

	if (gIOFBBlackBoot)
		gray = logo = 0;
	else if (gIOFBBlackBootTheme)
	    gray = 0;
	else if (32 == consoleDepth)
		gray = 0xbfbfbf;
	else if (30 == consoleDepth)
		gray = (rgb108(0xbf) | (rgb108(0xbf) << 10 ) | (rgb108(0xbf) << 20));
	else
		return;

	if (!logo) logoData = NULL;
	else
	{
		if (gIOFBBlackBootTheme)
		{
			if (2 == logo)
			{
				src      = &gAppleLogoBlack2XPacked[0];
				clut     = &gAppleLogoBlack2XClut[0];
				srcSize  = sizeof(gAppleLogoBlack2XPacked);
				clutSize = sizeof(gAppleLogoBlack2XClut) / 3;
			}
			else
			{
				src      = &gAppleLogoBlackPacked[0];
				clut     = &gAppleLogoBlackClut[0];
				srcSize  = sizeof(gAppleLogoBlackPacked);
				clutSize = sizeof(gAppleLogoBlackClut) / 3;
			}
		}
		else
		{
			if (2 == logo)
			{
				src      = &gAppleLogo2XPacked[0];
				clut     = &gAppleLogo2XClut[0];
				srcSize  = sizeof(gAppleLogo2XPacked);
				clutSize = sizeof(gAppleLogo2XClut) / 3;
			}
			else
			{
				src      = &gAppleLogoPacked[0];
				clut     = &gAppleLogoClut[0];
				srcSize  = sizeof(gAppleLogoPacked);
				clutSize = sizeof(gAppleLogoClut) / 3;
			}
		}

		lw  = (2 == logo) ? kAppleLogo2XWidth  : kAppleLogoWidth;
		lh  = (2 == logo) ? kAppleLogo2XHeight : kAppleLogoHeight;
		ox = (framebufferWidth - lw) / 2;
		oy = (framebufferHeight - lh) / 2;

		logoData = IONew(uint8_t, lw * lh);
		if (!logoData) logo = 0;
		else
		{
			unpackedSize = lzvn_decode(logoData, lw * lh, src, srcSize);
			if (unpackedSize != (lw * lh)) logo = 0;
		}
	}

	out = (uint32_t *) frameBuffer;
	for (y = 0; y < framebufferHeight; y++)
	{
		if ((!logo) || (y < oy) || (y >= (oy + lh)))
		{
			for (x = 0; x < framebufferWidth; x++) out[x] = gray;	
		}
		else
		{
			for (x = 0; x < ox; x++) out[x] = gray;
			for (sx = 0; sx < lw; sx++)
			{
				data = logoData[(y - oy) * lw + sx];
				if (data >= clutSize) data = gray;
				data *= 3;
				if (32 == consoleDepth)
				{
				    data = ((clut[data+0] << 16) | (clut[data+1] << 8) | clut[data+2]);
				}
				else if (30 == consoleDepth)
				{
					data = ((rgb108(clut[data+0]) << 20) | (rgb108(clut[data+1]) << 10) | rgb108(clut[data+2]));
				}
				out[x++] = data;
			}
			for (; x < framebufferWidth; x++) out[x] = gray;
		}
		out += totalWidth;
	}

	if (logoData) IODelete(logoData, uint8_t, lw * lh);
}

void IOFramebuffer::setupCursor(void)
{
    IOFB_START(setupCursor,0,0,0);
    StdFBShmem_t *              shmem = GetShmem(this);
	IOPixelInformation *        info  = &__private->pixelInfo;
    volatile unsigned char *    bits;
    IOByteCount                 cursorImageBytes, waitCursorImageBytes;

    rowBytes = info->bytesPerRow;
    fTotalWidth = (rowBytes * 8) / info->bitsPerPixel;
    bytesPerPixel = info->bitsPerPixel / 8;
    if (bytesPerPixel > 4)
        __private->cursorBytesPerPixel = 4;
    else
        __private->cursorBytesPerPixel = bytesPerPixel;

    fFrameBuffer = (volatile unsigned char *) (fVramMap ? fVramMap->getVirtualAddress() : 0);
    __private->framebufferWidth  = info->activeWidth;
    __private->framebufferHeight = info->activeHeight;
    if (info->bitsPerComponent > 8)
        __private->consoleDepth = info->componentCount * info->bitsPerComponent;
    else
        __private->consoleDepth = info->bitsPerPixel;

    if (shmem)
    {
		DEBG1(thisName, " setupCursor online %d, (%d, %d) - (%d, %d)\n", 
                __private->online,
                shmem->screenBounds.minx, shmem->screenBounds.miny,
                shmem->screenBounds.maxx, shmem->screenBounds.maxy);
        if (__private->online &&
                ((shmem->screenBounds.maxx == shmem->screenBounds.minx)
                || (shmem->screenBounds.maxy == shmem->screenBounds.miny)))
        {
            // a default if no one calls IOFBSetBounds()
            shmem->screenBounds.minx = 0;
            shmem->screenBounds.miny = 0;
            shmem->screenBounds.maxx = info->activeWidth;
            shmem->screenBounds.maxy = info->activeHeight;
			__private->screenBounds[0] = shmem->screenBounds;
			__private->screenBounds[1] = shmem->screenBounds;

			shmem->cursorSize[0] = maxCursorSize;
			shmem->cursorSize[1] = __private->maxWaitCursorSize;
			shmem->cursorSize[2] = __private->maxWaitCursorSize;
			shmem->cursorSize[3] = __private->maxWaitCursorSize;
        }
		__private->actualVBLCount = 0;

        cursorImageBytes = maxCursorSize.width * maxCursorSize.height
                           * __private->cursorBytesPerPixel;
        waitCursorImageBytes = __private->maxWaitCursorSize.width * __private->maxWaitCursorSize.height
                               * __private->cursorBytesPerPixel;
        bits = shmem->cursor;

        for (UInt32 i = 0; i < __private->numCursorFrames; i++)
        {
            __private->cursorFlags[i] = kIOFBCursorImageNew;
            __private->cursorImages[i] = bits;
            bits += i ? waitCursorImageBytes : cursorImageBytes;
        }
        if (info->bitsPerPixel <= 8)
        {
            for (UInt32 i = 0; i < __private->numCursorFrames; i++)
            {
                __private->cursorMasks[i] = bits;
                bits += i ? waitCursorImageBytes : cursorImageBytes;
            }
        }
        cursorSave = bits;
		cursorEnable = true;
    }

	cursorBlitProc = (CursorBlitProc) NULL;
	cursorRemoveProc = (CursorRemoveProc) NULL;
    if ((!haveHWCursor) && fVramMap) switch (info->bitsPerPixel)
    {
        case 8:
            if (colorConvert.t._bm256To38SampleTable
                    && colorConvert.t._bm38To256SampleTable)
            {
                cursorBlitProc = (CursorBlitProc) StdFBDisplayCursor8P;
                cursorRemoveProc = (CursorRemoveProc) StdFBRemoveCursor8;
            }
            break;
        case 16:
            if (colorConvert.t._bm34To35SampleTable
                    && colorConvert.t._bm35To34SampleTable)
            {
                cursorBlitProc = (CursorBlitProc) StdFBDisplayCursor555;
                cursorRemoveProc = (CursorRemoveProc) StdFBRemoveCursor16;
            }
            break;
        case 32:
        case 64:
			if (10 == info->bitsPerComponent)
				cursorBlitProc = (CursorBlitProc) StdFBDisplayCursor30Axxx;
			else
				cursorBlitProc = (CursorBlitProc) StdFBDisplayCursor32Axxx;
			cursorRemoveProc = (CursorRemoveProc) StdFBRemoveCursor32;
            break;
        default:
            break;
    }

	if ((!haveHWCursor) && (!cursorBlitProc)) DEBG1(thisName, " can't do sw cursor at depth %d\n",
                  				(uint32_t) info->bitsPerPixel);
    IOFB_END(setupCursor,0,0,0);
}

void IOFramebuffer::stopCursor( void )
{
    IOFB_START(stopCursor,0,0,0);
    cursorEnable = false;
    cursorBlitProc = (CursorBlitProc) NULL;
    cursorRemoveProc = (CursorRemoveProc) NULL;
    IOFB_END(stopCursor,0,0,0);
}

IOReturn IOFramebuffer::extCreateSharedCursor(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extCreateSharedCursor,0,0,0);
    IOFramebuffer * inst         = (IOFramebuffer *) target;
    int             cursorversion = static_cast<int>(args->scalarInput[0]);
    int             maxWidth     = static_cast<int>(args->scalarInput[1]);
    int             maxWaitWidth = static_cast<int>(args->scalarInput[2]);

    IOReturn err;

    if ((err = inst->extEntry(true, kIOGReportAPIState_CreateSharedCursor)))
    {
        IOFB_END(extCreateSharedCursor,err,__LINE__,0);
        return (err);
    }

	err = inst->createSharedCursor( cursorversion, maxWidth, maxWaitWidth );

    inst->extExit(err, kIOGReportAPIState_CreateSharedCursor);

    IOFB_END(extCreateSharedCursor,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::extCopySharedCursor(IOMemoryDescriptor **cursorH)
{
    FBGATEGUARD(ctrlgated, this);

    if (sharedCursor) {
        sharedCursor->retain();
        *cursorH = sharedCursor;
    }
    return kIOReturnSuccess;
}

IOReturn
IOFramebuffer::extCopyUserMemory(const uint32_t type, IOMemoryDescriptor** memP)
{
    IOReturn err = kIOReturnBadArgument;

    FBGATEGUARD(ctrlgated, this);

    IOMemoryDescriptor* mem = nullptr;
    if (static_cast<bool>(userAccessRanges))
        mem = OSDynamicCast(IOMemoryDescriptor,
                            userAccessRanges->getObject(type));
    if (static_cast<bool>(mem)) {
        mem->retain();
        *memP = mem;
        err = kIOReturnSuccess;
    }
    return err;
}

IOIndex IOFramebuffer::closestDepth(IODisplayModeID mode, IOPixelInformation * matchInfo)
{
    IOFB_START(closestDepth,mode,0,0);
	IOReturn err;
	IOIndex  depth;
	IOPixelInformation pixelInfo;

	depth = 0; 
	for (depth = 0; ; depth++)
	{
        FB_START(getPixelInformation,0,__LINE__,0);
		err = getPixelInformation(mode, depth, kIOFBSystemAperture, &pixelInfo);
        FB_END(getPixelInformation,err,__LINE__,0);
		if (kIOReturnSuccess != err)
		{
			depth = 0;
			break;
		}
		if ((pixelInfo.bitsPerPixel == matchInfo->bitsPerPixel)
		  && (pixelInfo.pixelType == matchInfo->pixelType)
		  && (pixelInfo.componentCount == matchInfo->componentCount)
		  && (pixelInfo.bitsPerComponent == matchInfo->bitsPerComponent))
			break;
	}
    IOFB_END(closestDepth,depth,0,0);
	return (depth);
}

IOReturn IOFramebuffer::extGetPixelInformation(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extGetPixelInformation,0,0,0);
    IOFramebuffer *      inst        = (IOFramebuffer *) target;
    IODisplayModeID      displayMode = static_cast<IODisplayModeID>(args->scalarInput[0]);
    IOIndex              depth       = static_cast<IOIndex>(args->scalarInput[1]);
    IOPixelAperture      aperture    = static_cast<IOPixelAperture>(args->scalarInput[2]);
    IOPixelInformation * pixelInfo   = (IOPixelInformation *) args->structureOutput;

    IOReturn err;

    if ((err = inst->extEntry(false, kIOGReportAPIState_GetPixelInformation)))
    {
        IOFB_END(extGetPixelInformation,err,__LINE__,0);
        return (err);
    }

    FB_START(getPixelInformation,0,__LINE__,0);
	err = inst->getPixelInformation(displayMode, depth, aperture, pixelInfo);
    FB_END(getPixelInformation,err,__LINE__,0);

    inst->extExit(err, kIOGReportAPIState_GetPixelInformation);

    IOFB_END(extGetPixelInformation,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::extGetCurrentDisplayMode(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extGetCurrentDisplayMode,0,0,0);
    IOFramebuffer * inst = (IOFramebuffer *) target;
    IODisplayModeID displayMode = 0;
    IOIndex         depth = 0;
    
    IOReturn err;
    
    if ((err = inst->extEntry(false, kIOGReportAPIState_GetCurrentDisplayMode)))
    {
        IOFB_END(extGetCurrentDisplayMode,err,__LINE__,0);
        return (err);
    }

    if (kIODisplayModeIDInvalid != inst->__private->aliasMode)
    {
        displayMode = inst->__private->aliasMode;
        depth       = inst->__private->currentDepth;
    }
    else
    {
        FB_START(getCurrentDisplayMode,0,__LINE__,0);
        err = inst->getCurrentDisplayMode(&displayMode, &depth);
        FB_END(getCurrentDisplayMode,err,__LINE__,0);
    }

#if 0
    IOG_KTRACE(DBG_IOG_GET_CURRENT_DISPLAY_MODE,
               DBG_FUNC_NONE,
               0, DBG_IOG_SOURCE_EXT_GET_CURRENT_DISPLAY_MODE,
               0, inst->__private->regID,
               0, GPACKUINT32T(1, depth) | GPACKUINT32T(0, displayMode),
               0, err);
#endif
    DEBG(inst->thisName, " displayMode 0x%08x, depth %d, aliasMode 0x%08x\n",
        displayMode, depth, inst->__private->aliasMode);

    inst->extExit(err, kIOGReportAPIState_GetCurrentDisplayMode);
    
    args->scalarOutput[0] = displayMode;
    args->scalarOutput[1] = depth;
    
    IOFB_END(extGetCurrentDisplayMode,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::extSetStartupDisplayMode(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extSetStartupDisplayMode,0,0,0);
    IOFramebuffer * inst        = (IOFramebuffer *) target;
    IODisplayModeID displayMode = static_cast<IODisplayModeID>(args->scalarInput[0]);
    IOIndex         depth       = static_cast<IOIndex>(args->scalarInput[1]);

    IOReturn err;

    if ((err = inst->extEntry(false, kIOGReportAPIState_SetStartupDisplayMode)))
    {
        IOFB_END(extSetStartupDisplayMode,err,__LINE__,0);
        return (err);
    }

    FB_START(setStartupDisplayMode,0,__LINE__,0);
	err = inst->setStartupDisplayMode( displayMode, depth );
    FB_END(setStartupDisplayMode,err,__LINE__,0);

	if (inst->__private->online && (kIODetailedTimingValid & inst->__private->timingInfo.flags))
	{
		OSData * data = OSData::withBytes(
			&inst->__private->timingInfo, sizeof(inst->__private->timingInfo));
		if (data)
		{
			inst->setPreference(NULL, gIOFBStartupModeTimingKey, data);
			data->release();
		}
	}

    inst->extExit(err, kIOGReportAPIState_SetStartupDisplayMode);
    
    IOFB_END(extSetStartupDisplayMode,err,0,0);
    return (err);
}

#if 0
static void IOFBLogGamma(
    uint32_t channelCount, uint32_t srcDataCount,
    uint32_t dataWidth, const void * data)
{
    if (data)
    {
        uint32_t c, i;
        uint16_t * words;
        words = ((uint16_t *)data);
        kprintf("/*    0      1      2      3      4      5      6      7      8      9      A      B      C      D      E      F */");
        for (c = 0; c < channelCount; c++)
        {
            for (i = 0; i < srcDataCount; i++)
            {
                if( 0 == (i & 15))
                    kprintf("\n    ");
                kprintf("0x%04x,", words[i]);
            }
            words += i;
            kprintf("\n\n\n");
        }
    } else kprintf("No gamma data present\n");
}
#endif

enum { kIOFBGammaPointCountMax = 256 };
enum { kIOFBGammaDesiredError  = 127 };

struct IOFBLineSeg
{
	// [start end]
    uint16_t start;
    uint16_t end;
    uint16_t dist;
    uint16_t split;
};
typedef struct IOFBLineSeg IOFBLineSeg;

static void 
IOFBSegDist(const uint16_t data[], IOFBLineSeg * seg)
{
	uint16_t start;
	uint16_t end;
	uint16_t idx;
	uint16_t interp;
	int16_t  dist;

	seg->dist  = 0;
	start = seg->start;
	end   = seg->end;
	for (idx = start + 1; idx < end; idx++)
	{
		interp = data[start] + ((data[end] - data[start]) * (idx - start)) / (end - start);
		dist = data[idx] - interp;
		if (dist < 0) dist = -dist;
		if (dist > seg->dist)
		{
			seg->dist = dist;
			seg->split = idx;
		}
	}
}

static void 
IOFBSegInit(const uint16_t data[], IOFBLineSeg * seg, uint16_t start, uint16_t end)
{
	seg->start = start;
	seg->end   = end;
	IOFBSegDist(data, seg);
}

static void
IOFBSimplifySegs(const uint16_t data[], uint16_t srcDataCount,
				uint16_t desiredError, uint16_t * maxError, 
				IOFBLineSeg * segs, uint16_t count, uint16_t maxCount,
				IOFBBootGamma * bootGamma)
{
	IOFBGamma * channelGamma;
	uint16_t    idx;
	uint16_t    furthest;
	uint16_t    start;

	while (count < maxCount)
	{
		furthest = 0;
		for (idx = 1; idx < count; idx++)
		{
			if (segs[idx].dist > segs[furthest].dist) furthest = idx;
		}
		if (segs[furthest].dist <= desiredError) break;
		bcopy(&segs[furthest+1], &segs[furthest+2], (count - furthest - 1) * sizeof(IOFBLineSeg));
		count++;
		IOFBSegInit(data, &segs[furthest+1], segs[furthest].split, segs[furthest].end);
		segs[furthest].end = segs[furthest].split;
		IOFBSegDist(data, &segs[furthest]);
	}

	*maxError = 0;
	channelGamma = &bootGamma->gamma.red;
	for (idx = 0; idx < count; idx++)
	{
		if (segs[idx].dist > *maxError) 
		{
			*maxError = segs[idx].dist;
			//IOLog("max error seg 0x%x, 0x%x count %d, target %d\n", segs[idx].start, segs[idx].end, count, maxCount);
		}
		start = segs[idx].start % srcDataCount;
		if (!start)
		{
			if (segs[idx].start) 
			{ 
				channelGamma = (IOGRAPHICS_TYPEOF(channelGamma)) &channelGamma->points[channelGamma->pointCount];
			}
			channelGamma->pointCount = 0;
			continue;
		}
		channelGamma->points[channelGamma->pointCount].in = 
						(start * 65536 / srcDataCount)
						 | (start * 65536 / srcDataCount / srcDataCount);
		channelGamma->points[channelGamma->pointCount].out = data[segs[idx].start];
		channelGamma->pointCount++;
	}
}

static bool 
IOFBCompressGamma(
	IOFBBootGamma * bootGamma,
    uint16_t channelCount, uint16_t srcDataCount,
    uint16_t dataWidth, const void * _data,
    uint16_t desiredError, uint16_t maxCount,
    uint16_t * maxError)
{
	IOFBGamma *      channelGamma;
	const uint16_t * data = (IOGRAPHICS_TYPEOF(data)) _data;
	IOFBLineSeg *    segs;
	uint16_t         idx;

	if ((3 != channelCount) || (16 != dataWidth)) return (false);

	maxCount += 3;
	segs = IONew(IOFBLineSeg, maxCount);
	if (!segs) return (false);

	for (idx = 0; idx < 3; idx++) IOFBSegInit(data, &segs[idx], idx * srcDataCount, ((idx + 1) * srcDataCount) - 1);

	IOFBSimplifySegs(data, srcDataCount, desiredError, maxError,
					segs, 3, maxCount,
					bootGamma);

	IODelete(segs, IOFBLineSeg, maxCount);

	channelGamma = &bootGamma->gamma.red;
	for (idx = 0; idx < channelCount; idx++)
	{
		channelGamma = (IOGRAPHICS_TYPEOF(channelGamma)) &channelGamma->points[channelGamma->pointCount];
	}
	bootGamma->length = ((uintptr_t) channelGamma) - ((uintptr_t) bootGamma);

	return (true);
}


static void __unused
IOFBDecompressGamma(const IOFBBootGamma * bootGamma, uint16_t * data, uint16_t count) 
{
	const IOFBGamma * channelGamma;
	uint16_t channel, idx, maxIdx, seg;
	uint16_t startIn, startOut, point;
	uint16_t endIn, endOut;

	maxIdx = count - 1;
	channelGamma = &bootGamma->gamma.red;
	for (channel = 0; channel < 3; channel++)
	{
		seg = 0;
		startIn = 0;
		startOut = 0x0000;
		endIn = 0;
		endOut = 0;
		for (idx = 0; idx <= maxIdx; idx++)
		{
			point = idx * 65535 / maxIdx;
			if ((point >= endIn) && (idx != maxIdx))
			{
				startIn = endIn;
				startOut = endOut;
				if (seg < channelGamma->pointCount)
				{
					endIn  = channelGamma->points[seg].in;
					endOut = channelGamma->points[seg].out;
					seg++;
				}
				else endIn = endOut = 0xFFFF;
			}
			data[channel * count + idx] = startOut + ((endOut - startOut) * (point - startIn)) / (endIn - startIn);
		}
		channelGamma = (IOGRAPHICS_TYPEOF(channelGamma)) &channelGamma->points[channelGamma->pointCount];
	}
}

void IOFramebuffer::saveGammaTables(void)
{
    IOFB_START(saveGammaTables,0,0,0);
	IORegistryEntry * options;
	const OSSymbol *  sym;
	IOFBBootGamma *   bootGamma;
	IOFramebuffer *   fb;
	OSNumber *        num;
	OSData *          data;
	uint32_t          maxCount;
	uint16_t          maxError;

	options = IORegistryEntry::fromPath("/options", gIODTPlane);
	if (!options)
    {
        IOFB_END(saveGammaTables,-1,__LINE__,0);
        return;
    }

	maxCount = (gIOFBBacklightDisplayCount + gIOFBDisplayCount);
	if (!maxCount)
    {
        IOFB_END(saveGammaTables,-1,__LINE__,0);
        return;
    }
	maxCount = ((0x700 - (maxCount * sizeof(IOFBBootGamma))) / sizeof(IOFBGammaPoint) / maxCount);
	if (maxCount > kIOFBGammaPointCountMax) maxCount = kIOFBGammaPointCountMax;

	bootGamma = (IOGRAPHICS_TYPEOF(bootGamma)) IOMalloc(sizeof(IOFBBootGamma) 
					+ maxCount * sizeof(IOFBGammaPoint));
	if (!bootGamma)
    {
        IOFB_END(saveGammaTables,-1,__LINE__,0);
        return;
    }

	data = OSData::withCapacity(512);
	if (!data)
    {
        IOFB_END(saveGammaTables,-1,__LINE__,0);
        return;
    }

    FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
	{
        FBGATEGUARD(ctrlgated, fb);
		if (fb->__private->display)
		{
			bootGamma->vendor  = 0;
			bootGamma->product = 0;
			bootGamma->serial  = 0;
			bootGamma->length  = 0;
			bootGamma->resvA   = 0;
			bootGamma->resvB   = 0;

			if ((num = OSDynamicCast(OSNumber, fb->__private->display->getProperty(kDisplayVendorID))))
				bootGamma->vendor  = num->unsigned32BitValue();
			if ((num = OSDynamicCast(OSNumber, fb->__private->display->getProperty(kDisplayProductID))))
				bootGamma->product = num->unsigned32BitValue();
			if ((num = OSDynamicCast(OSNumber, fb->__private->display->getProperty(kDisplaySerialNumber))))
				bootGamma->serial  = num->unsigned32BitValue();
#if 0
        	IOFBLogGamma(fb->__private->rawGammaChannelCount, 
        				 fb->__private->rawGammaDataCount, 
        				 fb->__private->rawGammaDataWidth, 
        				 fb->__private->rawGammaData);
#endif
        	if (IOFBCompressGamma(bootGamma, 
        							fb->__private->rawGammaChannelCount, 
        							fb->__private->rawGammaDataCount, 
        							fb->__private->rawGammaDataWidth, 
        							fb->__private->rawGammaData,
        						    kIOFBGammaDesiredError, maxCount, 
        						    &maxError))
			{
				DEBG1(fb->thisName, " compressed gamma %d max error 0x%04x\n", bootGamma->length, maxError);
				if (bootGamma->gamma.red.pointCount)
				{
					data->appendBytes(bootGamma, bootGamma->length);
				}
			}

		}
	}
	IOFree(bootGamma, sizeof(IOFBBootGamma) 
					+ maxCount * sizeof(IOFBGammaPoint));
	sym = OSSymbol::withCStringNoCopy(kIOFBBootGammaKey);
	if (sym && data->getLength())
	{
		options->setProperty(sym, data);
		sym->release();
	}
	data->release();
	options->release();

    IOFB_END(saveGammaTables,0,0,0);
}

IOReturn allocateGammaBuffer(const uint32_t & channelCount,
                             const uint32_t & dataCount,
                             const uint32_t & dataWidth,
                             IOByteCount & dataLen,
                             IOByteCount & gammaDataLen,
                             uint8_t * & gammaData);
IOReturn allocateGammaBuffer(const uint32_t & channelCount,
                             const uint32_t & dataCount,
                             const uint32_t & dataWidth,
                             IOByteCount & dataLen,
                             IOByteCount & gammaDataLen,
                             uint8_t * & gammaData)
{
    IOReturn err = kIOReturnSuccess;

    dataLen  = (dataWidth + 7) / 8;
    dataLen *= dataCount * channelCount;

    if (dataLen != gammaDataLen)
    {
        if (gammaDataLen)
        {
            IODelete(gammaData, uint8_t, gammaDataLen);
        }
        gammaData = IONew(uint8_t, dataLen);
        gammaDataLen = dataLen;
    }

    if (NULL == gammaData)
    {
        err = kIOReturnNoMemory;
        gammaDataLen = 0;
    }

    return (err);
}

IOReturn IOFramebuffer::extSetHibernateGammaTable(
     OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer       * inst          = OSDynamicCast(IOFramebuffer, target);
    if (NULL != inst)
    {
        IOFB_START(extSetHibernateGammaTable,inst->__private->regID,0,0);
    }
    else
    {
        IOFB_START(extSetHibernateGammaTable,-1,0,0);
    }

    const uint32_t      channelCount    = static_cast<const uint32_t>(args->scalarInput[0]);
    const uint32_t      dataCount       = static_cast<const uint32_t>(args->scalarInput[1]);
    const uint32_t      dataWidth       = static_cast<const uint32_t>(args->scalarInput[2]);
    IOByteCount         dataLen;
    IOReturn            err;

    if ((NULL == inst) || (channelCount != 3)
        || (0 == dataCount) || (dataCount > 1024)
        || (0 == dataWidth) || (dataWidth > 16))
    {
        IOFB_END(extSetHibernateGammaTable,kIOReturnBadArgument,0,0);
        return (kIOReturnBadArgument);
    }

    if ((err = inst->extEntry(true, kIOGReportAPIState_SetHibernateGammaTable)))
    {
        IOFB_END(extSetHibernateGammaTable,err,__LINE__,0);
        return (err);
    }

    err = allocateGammaBuffer(channelCount,
                              dataCount,
                              dataWidth,
                              dataLen,
                              inst->__private->hibernateGammaDataLen,
                              inst->__private->hibernateGammaData);
    
    if (kIOReturnSuccess == err)
    {
        inst->__private->hibernateGammaChannelCount = channelCount;
        inst->__private->hibernateGammaDataCount    = dataCount;
        inst->__private->hibernateGammaDataWidth    = dataWidth;

        if (args->structureInputDescriptor)
        {
            if (dataLen != args->structureInputDescriptor->getLength())
            {
                err = kIOReturnBadArgument;
            }
            else
            {
                err = args->structureInputDescriptor->prepare(kIODirectionOut);
                if ((kIOReturnSuccess == err) &&
                    (dataLen != args->structureInputDescriptor->readBytes(
                        0, inst->__private->hibernateGammaData, dataLen)))
                {
                    err = kIOReturnVMError;
                }
                args->structureInputDescriptor->complete();
            }
        }
        else
        {
            if (dataLen == args->structureInputSize)
            {
                bcopy(args->structureInput, inst->__private->hibernateGammaData, dataLen);
            }
            else
            {
                err = kIOReturnBadArgument;
            }
        }
#if 0
        IOFBLogGamma(inst->__private->hibernateGammaChannelCount,
                     inst->__private->hibernateGammaDataCount,
                     inst->__private->hibernateGammaDataWidth,
                     inst->__private->hibernateGammaData);
#endif

    }

    inst->extExit(err,kIOGReportAPIState_SetHibernateGammaTable);

    IOFB_END(extSetHibernateGammaTable,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::extSetGammaTable(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extSetGammaTable,0,0,0);
    IOFramebuffer * inst            = (IOFramebuffer *) target;
    UInt32          channelCount    = static_cast<UInt32>(args->scalarInput[0]);
    UInt32          dataCount       = static_cast<UInt32>(args->scalarInput[1]);
    UInt32          dataWidth       = static_cast<UInt32>(args->scalarInput[2]);
    SInt32          syncType        = static_cast<SInt32>(args->scalarInput[3]);
    bool            bImmediate      = static_cast<bool>(args->scalarInput[4]);
    IOReturn        err;
    IOByteCount     dataLen;

    // <rdar://problem/23114787> Integer overflow in IOFramebuffer::extSetGammaTable
    // <rdar://problem/23814363> IOFramebuffer::extSetGammaTable integer overflow when calculating dataLen
    // <rdar://problem/30805056> PanicTracer: 42 panics at IOFramebuffer::updateGammaTable does not check correct length on fast path.
    //     Only allow a channelCount of 3, otherwise fast path may panic.
    if ((channelCount != 3)
    || (0 == dataCount) || (dataCount > 1024)
    || (0 == dataWidth) || (dataWidth > 16))
    {
        IOFB_END(extSetGammaTable,kIOReturnBadArgument,0,0);
        return kIOReturnBadArgument;
    }

    if ((err = inst->extEntry(true, kIOGReportAPIState_SetGammaTable)))
    {
        IOFB_END(extSetGammaTable,err,__LINE__,0);
        return (err);
    }

    err = allocateGammaBuffer(channelCount,
                              dataCount,
                              dataWidth,
                              dataLen,
                              inst->__private->rawGammaDataLen,
                              inst->__private->rawGammaData);

    if (kIOReturnSuccess == err)
    {
        inst->__private->rawGammaChannelCount = channelCount;
        inst->__private->rawGammaDataCount    = dataCount;
        inst->__private->rawGammaDataWidth    = dataWidth;

        if (args->structureInputDescriptor)
        {
            if (dataLen != args->structureInputDescriptor->getLength())
                err = kIOReturnBadArgument;
            else
            {
                err = args->structureInputDescriptor->prepare(kIODirectionOut);
                if ((kIOReturnSuccess == err)
                 && (dataLen != args->structureInputDescriptor->readBytes(
                                0, inst->__private->rawGammaData, dataLen)))
                {
                    err = kIOReturnVMError;
                }
                args->structureInputDescriptor->complete();
            }
        }
        else
        {
            if (dataLen == args->structureInputSize)
                bcopy(args->structureInput, inst->__private->rawGammaData, dataLen);
            else
                err = kIOReturnBadArgument;
        }
        if (kIOReturnSuccess == err)
        {
#if 0
        	IOFBLogGamma(inst->__private->rawGammaChannelCount, 
        				 inst->__private->rawGammaDataCount, 
        				 inst->__private->rawGammaDataWidth, 
        				 inst->__private->rawGammaData);
#endif
            err = inst->updateGammaTable(channelCount,
                                         dataCount,
                                         dataWidth,
                                         inst->__private->rawGammaData,
                                         syncType,
                                         bImmediate,
                                         true /*ignoreTransactionActive*/ );
        }
    }
#if 0
DEBG1(inst->thisName, " extSetGammaTable(%x) online %d %ld %ld data %x\n", 
	err, inst->__private->online, 
	channelCount, dataCount,
	*((uint32_t *) inst->__private->rawGammaData));
#endif

	if (!inst->__private->gammaSet)
	{
		gIOFBGrayValue = kIOFBGrayValue;
	    inst->__private->gammaSet = 1;
	}

    inst->extExit(err,kIOGReportAPIState_SetGammaTable);

    IOFB_END(extSetGammaTable,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::updateGammaTable(
    UInt32 channelCount, UInt32 srcDataCount,
    UInt32 dataWidth, const void * data,
    SInt32 syncType, bool immediate,
    bool ignoreTransactionActive)
{
    IOFB_START(updateGammaTable,channelCount,syncType,immediate);
    IOReturn    err = kIOReturnBadArgument;
    IOByteCount dataLen;
    UInt16 *    channelData;
    UInt32      dataCount;
    UInt32      tryWidth;
    UInt8 *     table = NULL;
    bool        needAlloc;
    bool        gammaHaveScale = ((1 << 16) != __private->gammaScale[0])
        || ((1 << 16) != __private->gammaScale[1])
        || ((1 << 16) != __private->gammaScale[2])
        || ((1 << 16) != __private->gammaScale[3]);
    const uint32_t * adjustParams = NULL;
    const uint32_t * adjustNext   = NULL;
    uint32_t         gammaThresh;
    uint32_t         gammaAdjust;
    const bool  dropGammaSet = ((!ignoreTransactionActive) &&
                                __private->transactionsEnabled);

    if (GAMMA_ADJ && gIOGraphicsControl && (__private->desiredGammaDataWidth <= 8))
    {
        static const uint32_t _params[]  = { 138, 3, 256, 4 };
        adjustParams = &_params[0];
    }
    else
    {
        gammaThresh = -1U;
        gammaAdjust = 0;
    }
    do
    {
        if (!__private->online)
        {
            DEBG1(thisName, " offline updateGammaTable\n");
            err = kIOReturnOffline;
            break;
        }
        if (dataWidth != 16)
            break;
        if (channelCount != 3)
            break;

        if (!__private->desiredGammaDataWidth)
        {
            __private->desiredGammaDataWidth = dataWidth;
            __private->desiredGammaDataCount = srcDataCount;
        }

        dataCount = __private->desiredGammaDataCount;
        dataLen   = (__private->desiredGammaDataWidth + 7) / 8;
        dataLen  *= dataCount * channelCount;
        dataLen  += __private->gammaHeaderSize;

        needAlloc = (0 == __private->gammaDataLen);
        if (!needAlloc)
        {
            table = __private->gammaData;
            if (__private->gammaDataLen != dataLen)
            {
                IODelete(table, UInt8, __private->gammaDataLen);
                __private->gammaData = NULL;
                __private->gammaDataLen = 0;
                needAlloc = true;
            }
            __private->gammaNeedSet = false;
        }

        if (needAlloc)
        {
            table = IONew(UInt8, dataLen);
            if (!table)
            {
                err = kIOReturnNoMemory;
                continue;
            }
            __private->gammaData = table;
        }

        table += __private->gammaHeaderSize;
        tryWidth = __private->desiredGammaDataWidth;

        if (!gammaHaveScale && data && !adjustParams
        &&  (__private->desiredGammaDataCount == srcDataCount)
        &&  (__private->desiredGammaDataWidth == dataWidth))
        {
            const IOByteCount len = dataLen - __private->gammaHeaderSize;
            assert(len == __private->rawGammaDataLen);
            bcopy(data, table, len);
        }
        else
        {
            uint32_t pin, pt5, in, out, channel, idx, maxSrc, maxDst, interpCount;
            int64_t value, value2;

            pin = (1 << tryWidth) - 1;
            pt5 = 0; //(1 << (tryWidth - 1));               // truncate not round
            if (gammaHaveScale)
                dataWidth += 32;

            channelData = (UInt16 *) data;
            maxSrc = (srcDataCount - 1);
            maxDst = (__private->desiredGammaDataCount - 1);
            if ((srcDataCount < __private->desiredGammaDataCount)
            &&  (0 == (__private->desiredGammaDataCount % srcDataCount)))
                interpCount = __private->desiredGammaDataCount / srcDataCount;
            else
                interpCount = 0;

            for (out = 0, channel = 0; channel < channelCount; channel++)
            {
                if (adjustParams)
                {
                    gammaThresh = 0;
                    adjustNext = adjustParams;
                }
                for (idx = 0; idx <= maxDst; idx++)
                {
                    if (idx >= gammaThresh)
                    {
                        gammaThresh = *adjustNext++;
                        gammaAdjust = *adjustNext++;
                    }
                    if (channelData)
                    {
                        in = ((idx * maxSrc) + (idx ? (idx - 1) : 0)) / maxDst;
                        value = (channelData[in] /*+ pt5*/);
                        if (interpCount && (in < maxSrc))
                        {
                            value2 = (channelData[in+1] /*+ pt5*/);
                            value += ((value2 - value) * (idx % interpCount) + (interpCount - 1)) / interpCount;
                        }
                    }
                    else
                        value = (idx * ((1 << dataWidth) - 1)) / maxDst;
                    if (gammaHaveScale)
                    {
                        value = ((value * __private->gammaScale[channel] * __private->gammaScale[3]) + (1U << 31));
                    }
                    value = (value >> (dataWidth - tryWidth));
                    if (value)
                        value += gammaAdjust;
                    if (value > pin)
                        value = pin;

                    if (tryWidth <= 8)
                        ((UInt8 *) table)[out] = (value & 0xff);
                    else
                        ((UInt16 *) table)[out] = value;
                    out++;
                }
                if (channelData) channelData += srcDataCount;
            }
        }
        __private->gammaChannelCount = channelCount;
        __private->gammaDataCount    = dataCount;
        __private->gammaDataLen      = dataLen;
        __private->gammaDataWidth    = tryWidth;
        __private->gammaSyncType     = syncType;

        if (ASYNC_GAMMA && !immediate)
        {
            if (__private->vblThrottle && __private->deferredCLUTSetTimerEvent)
            {
                AbsoluteTime deadline;
                getTimeOfVBL(&deadline, 1);
                __private->deferredCLUTSetTimerEvent->wakeAtTime(deadline);
                __private->gammaNeedSet = true;
                err = kIOReturnSuccess;
            }
            else if (__private->deferredCLUTSetEvent)
            {
                __private->gammaNeedSet = true;
                err = kIOReturnSuccess;
            }
        }
#if 0
        OSData * ddata;
        if (data)
        {
            ddata = OSData::withBytes((void *)data, srcDataCount*channelCount*sizeof(UInt16));
            if (ddata)
            {
                setProperty("GammaIn", ddata);
                ddata->release();
            }
        }
        ddata = OSData::withBytesNoCopy(table, dataLen - __private->gammaHeaderSize);
        if (ddata)
        {
            setProperty("GammaOut", ddata);
            ddata->release();
        }
#endif

        if (!__private->gammaNeedSet && !dropGammaSet)
        {
            // If CD sent us a sync request, try with new, else fallback.
            if (kIOFBSetGammaSyncNotSpecified != __private->gammaSyncType)
            {
                FB_START(setGammaTable2,0,__LINE__,0);
                IOG_KTRACE(DBG_IOG_SET_GAMMA_TABLE, DBG_FUNC_START,
                           0, __private->regID,
                           0, DBG_IOG_SOURCE_UPDATE_GAMMA_TABLE,
                           0, 0,
                           0, 0);
                err = setGammaTable(
                        __private->gammaChannelCount, __private->gammaDataCount,
                        __private->gammaDataWidth, __private->gammaData,
                        kIOFBSetGammaSyncNoSync != __private->gammaSyncType);
                IOG_KTRACE(DBG_IOG_SET_GAMMA_TABLE, DBG_FUNC_END,
                           0, __private->regID,
                           0, DBG_IOG_SOURCE_UPDATE_GAMMA_TABLE,
                           0, err,
                           0, 0);
                FB_END(setGammaTable2,err,__LINE__,0);
            }
            else
            {
                err = kIOReturnUnsupported;
            }

            if (kIOReturnUnsupported == err)
            {
                FB_START(setGammaTable,0,__LINE__,0);
                IOG_KTRACE(DBG_IOG_SET_GAMMA_TABLE, DBG_FUNC_START,
                           0, __private->regID,
                           0, DBG_IOG_SOURCE_UPDATE_GAMMA_TABLE,
                           0, 0,
                           0, 0);
                err = setGammaTable(
                        __private->gammaChannelCount, __private->gammaDataCount,
                        __private->gammaDataWidth, __private->gammaData );
                IOG_KTRACE(DBG_IOG_SET_GAMMA_TABLE, DBG_FUNC_END,
                           0, __private->regID,
                           0, DBG_IOG_SOURCE_UPDATE_GAMMA_TABLE,
                           0, err,
                           0, 0);
                FB_END(setGammaTable,err,__LINE__,0);
            }
            updateCursorForCLUTSet();
        }
    }
    while (false);

    IOFB_END(updateGammaTable,err,0,0);
    return (err);
}


IOReturn IOFramebuffer::extSetCLUTWithEntries(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extSetCLUTWithEntries,0,0,0);
    IOFramebuffer * inst    = (IOFramebuffer *) target;
    UInt32          index   = static_cast<UInt32>(args->scalarInput[0]);
    IOOptionBits    options = static_cast<IOOptionBits>(args->scalarInput[1]);
    IOColorEntry *  colors  = (IOColorEntry *) args->structureInput;
    IOByteCount     dataLen = args->structureInputSize;

    IOReturn    err;
    UInt8 *     table;
    bool        needAlloc;

    if ((err = inst->extEntry(false, kIOGReportAPIState_SetCLUTWithEntries)))
    {
        IOFB_END(extSetCLUTWithEntries,err,__LINE__,0);
        return (err);
    }

    err = kIOReturnBadArgument;
    if (inst->__private->deferredCLUTSetEvent)
    {
        do
        {
            needAlloc = (0 == inst->__private->clutDataLen);
            if (!needAlloc)
            {
                if (index || (inst->__private->clutDataLen != dataLen))
                {
                    inst->checkDeferredCLUTSet();
                    needAlloc = true;
                }
                inst->__private->clutDataLen = 0;
            }
    
            if (needAlloc)
            {
                table = IONew(UInt8, dataLen);
                if (!table)
                {
                    err = kIOReturnNoMemory;
                    continue;
                }
                inst->__private->clutData = table;
            }
            else
                table = inst->__private->clutData;
        
            inst->__private->clutIndex   = index;
            inst->__private->clutOptions = options;
            inst->__private->clutDataLen = dataLen;
    
            bcopy(colors, table, dataLen);

			if (inst->__private->vblThrottle && inst->__private->deferredCLUTSetTimerEvent)
			{
				AbsoluteTime deadline;
				inst->getTimeOfVBL(&deadline, 1);
				inst->__private->deferredCLUTSetTimerEvent->wakeAtTime(deadline);
			}

            err = kIOReturnSuccess;
        }
        while (false);
    }
    else
    {
        FB_START(setCLUTWithEntries,0,__LINE__,0);
        err = inst->setCLUTWithEntries( colors, index,
                                  static_cast<UInt32>(dataLen / sizeof(IOColorEntry)), options );
        FB_END(setCLUTWithEntries,err,__LINE__,0);
        inst->updateCursorForCLUTSet();
    }

    if (inst == gIOFBConsoleFramebuffer)
    {
        UInt32 inIdx, count = static_cast<UInt32>(index + dataLen / sizeof(IOColorEntry));
        if (count > 256) count = 256;
        for (inIdx = 0; index < count; index++, inIdx++)
        {
            appleClut8[index * 3 + 0] = colors[inIdx].red   >> 8;
            appleClut8[index * 3 + 1] = colors[inIdx].green >> 8;
            appleClut8[index * 3 + 2] = colors[inIdx].blue  >> 8;
        }
    }

    inst->extExit(err, kIOGReportAPIState_SetCLUTWithEntries);

    IOFB_END(extSetCLUTWithEntries,err,0,0);
    return (err);
}

void IOFramebuffer::updateCursorForCLUTSet( void )
{
    if (__private->cursorClutDependent)
    {
        StdFBShmem_t *shmem = GetShmem(this);

        SETSEMA(shmem);
        if ((kIOFBHardwareCursorActive == shmem->hardwareCursorActive)
         && !shmem->cursorShow)
        {
            RemoveCursor(this);
            FB_START(setCursorImage,0,__LINE__,0);
            setCursorImage( (void *)(uintptr_t) shmem->frame );
            FB_END(setCursorImage,0,__LINE__,0);
            DisplayCursor(this);
        }
        CLEARSEMA(shmem, this);
    }
}

void IOFramebuffer::deferredCLUTSetInterrupt( OSObject * owner,
                                              IOInterruptEventSource * evtSrc, int intCount )
{
    IOFB_START(deferredCLUTSetInterrupt,intCount,0,0);
    IOFramebuffer * fb = (IOFramebuffer *) owner;

    // Pretty sure the waitVBLEvent is meaningless, the other side is commented
    // out now.
    if (fb->__private->waitVBLEvent)
    {
        FBWL(fb)->wakeupGate(fb->__private->waitVBLEvent, true);
        fb->__private->waitVBLEvent = 0;
    }

    fb->checkDeferredCLUTSet();
    IOFB_END(deferredCLUTSetInterrupt,0,0,0);
}

void IOFramebuffer::deferredCLUTSetTimer(OSObject * owner, IOTimerEventSource * source)
{
    IOFB_START(deferredCLUTSetTimer,0,0,0);
    IOFramebuffer * fb = (IOFramebuffer *) owner;
    fb->checkDeferredCLUTSet();
    IOFB_END(deferredCLUTSetTimer,0,0,0);
}

void IOFramebuffer::checkDeferredCLUTSet( void )
{
    IOFB_START(checkDeferredCLUTSet,0,0,0);
    IOReturn    ret;
    bool        gammaNeedSet = __private->gammaNeedSet;
    IOByteCount clutLen      = __private->clutDataLen;

    if( !gammaNeedSet && !clutLen)
    {
        IOFB_END(checkDeferredCLUTSet,-1,0,0);
        return;
    }

    __private->gammaNeedSet = false;
    __private->clutDataLen  = 0;

    if (gammaNeedSet)
    {
        if (kIOFBSetGammaSyncNotSpecified != __private->gammaSyncType)
        {
            FB_START(setGammaTable2,0,__LINE__,0);
            IOG_KTRACE(DBG_IOG_SET_GAMMA_TABLE, DBG_FUNC_START,
                       0, __private->regID,
                       0, DBG_IOG_SOURCE_DEFERRED_CLUT,
                       0, 0,
                       0, 0);
            ret = setGammaTable( __private->gammaChannelCount, __private->gammaDataCount,
                                __private->gammaDataWidth, __private->gammaData,
                                (kIOFBSetGammaSyncNoSync == __private->gammaSyncType) ? false : true );
            IOG_KTRACE(DBG_IOG_SET_GAMMA_TABLE, DBG_FUNC_END,
                       0, __private->regID,
                       0, DBG_IOG_SOURCE_DEFERRED_CLUT,
                       0, ret,
                       0, 0);
            FB_END(setGammaTable2,ret,__LINE__,0);
        }
        else
        {
            ret = kIOReturnUnsupported;
        }

        if (kIOReturnUnsupported == ret)
        {
            FB_START(setGammaTable,0,__LINE__,0);
            IOG_KTRACE(DBG_IOG_SET_GAMMA_TABLE, DBG_FUNC_START,
                       0, __private->regID,
                       0, DBG_IOG_SOURCE_DEFERRED_CLUT,
                       0, 0,
                       0, 0);
            ret = setGammaTable( __private->gammaChannelCount, __private->gammaDataCount,
                                __private->gammaDataWidth, __private->gammaData );
            IOG_KTRACE(DBG_IOG_SET_GAMMA_TABLE, DBG_FUNC_END,
                       0, __private->regID,
                       0, DBG_IOG_SOURCE_DEFERRED_CLUT,
                       0, ret,
                       0, 0);
            FB_END(setGammaTable,ret,__LINE__,0);
        }
    }

    if (clutLen)
    {
        FB_START(setCLUTWithEntries,0,__LINE__,0);
        ret = setCLUTWithEntries( (IOColorEntry *) __private->clutData, __private->clutIndex,
                                  static_cast<UInt32>(clutLen / sizeof(IOColorEntry)), __private->clutOptions );
        FB_END(setCLUTWithEntries,ret,__LINE__,0);

        IODelete(__private->clutData, UInt8, clutLen);
    }

    updateCursorForCLUTSet();
    IOFB_END(checkDeferredCLUTSet,0,0,0);
}

IOReturn IOFramebuffer::createSharedCursor(
    int cursorversion, int maxWidth, int maxWaitWidth )
{
    FBASSERTGATED(this);

    IOFB_START(createSharedCursor,cursorversion,maxWidth,maxWaitWidth);
    StdFBShmem_t *      shmem;
    UInt32              shmemVersion;
    size_t              size, maxImageSize, maxWaitImageSize;
    UInt32              numCursorFrames;

    DEBG(thisName, " vers = %08x, %d x %d\n",
         cursorversion, maxWidth, maxWaitWidth);

    shmemVersion = cursorversion & kIOFBShmemVersionMask;

    const auto uMaxWidth = static_cast<uint32_t>(maxWidth);
    const auto uMaxWaitWidth = static_cast<uint32_t>(maxWaitWidth);
    if (uMaxWidth > kIOFBMaxCursorWidth || uMaxWaitWidth > kIOFBMaxCursorWidth)
        return (kIOReturnNoMemory);

    if (shmemVersion == kIOFBTenPtTwoShmemVersion)
    {
        numCursorFrames = (kIOFBShmemCursorNumFramesMask & cursorversion) >> kIOFBShmemCursorNumFramesShift;
        if (numCursorFrames > kIOFBMaxCursorFrames)
            return (kIOReturnNoMemory);

        setProperty(kIOFBWaitCursorFramesKey, (numCursorFrames - 1), 32);
        setProperty(kIOFBWaitCursorPeriodKey, 33333333, 32);    /* 30 fps */
    }
    else if (shmemVersion == kIOFBTenPtOneShmemVersion)
    {
        numCursorFrames = 4;
    }
    else
    {
        IOFB_END(createSharedCursor,kIOReturnUnsupported,__LINE__,0);
        return (kIOReturnUnsupported);
    }

    shmemClientVersion = shmemVersion;

    if (__private->cursorFlags)
    {
        IODelete( __private->cursorFlags, UInt8, __private->numCursorFrames );
        __private->cursorFlags = 0;
    }
    if (__private->cursorImages)
    {
        IODelete( __private->cursorImages, volatile unsigned char *, __private->numCursorFrames );
        __private->cursorImages = 0;
    }
    if (__private->cursorMasks)
    {
        IODelete( __private->cursorMasks, volatile unsigned char *, __private->numCursorFrames );
        __private->cursorMasks = 0;
    }
    __private->numCursorFrames = numCursorFrames;
    __private->cursorFlags     = IONew( UInt8, numCursorFrames );
    __private->cursorImages    = IONew( volatile unsigned char *, numCursorFrames );
    __private->cursorMasks     = IONew( volatile unsigned char *, numCursorFrames );

    if (!__private->cursorFlags || !__private->cursorImages || !__private->cursorMasks)
    {
        IOFB_END(createSharedCursor,kIOReturnNoMemory,__LINE__,0);
        return (kIOReturnNoMemory);
    }

    bzero(__private->cursorFlags,  numCursorFrames * sizeof(UInt8));
    bzero(__private->cursorImages, numCursorFrames * sizeof(volatile unsigned char *));
    bzero(__private->cursorMasks,  numCursorFrames * sizeof(volatile unsigned char *));

    maxImageSize = (maxWidth * maxWidth * kIOFBMaxCursorDepth) / 8;
    maxWaitImageSize = (maxWaitWidth * maxWaitWidth * kIOFBMaxCursorDepth) / 8;

    size = sizeof( StdFBShmem_t)
           + maxImageSize
           + max(maxImageSize, maxWaitImageSize)
           + ((numCursorFrames - 1) * maxWaitImageSize);

    if (!sharedCursor)
    {
        priv = NULL;
        sharedCursor = IOBufferMemoryDescriptor::withOptions(
                kIODirectionNone | kIOMemoryKernelUserShared, size );
        if (!sharedCursor)
        {
            IOFB_END(createSharedCursor,kIOReturnNoMemory,__LINE__,0);
            return (kIOReturnNoMemory);
        }
    }
    shmem = (StdFBShmem_t *) sharedCursor->getBytesNoCopy();
    priv = shmem;

    // Init shared memory area
    bzero( shmem, size );
	shmem->version = shmemClientVersion;
    shmem->structSize = static_cast<int>(size);
    shmem->cursorShow = 1;
    shmem->hardwareCursorCapable = haveHWCursor;
    for (UInt32 i = 0; i < numCursorFrames; i++)
        __private->cursorFlags[i] = kIOFBCursorImageNew;

    maxCursorSize.width = maxWidth;
    maxCursorSize.height = maxWidth;
    __private->maxWaitCursorSize.width = maxWaitWidth;
    __private->maxWaitCursorSize.height = maxWaitWidth;

    doSetup( false );

    IOFB_END(createSharedCursor,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::setBoundingRect( IOGBounds * bounds )
{
    IOFB_START(setBoundingRect,0,0,0);
    IOFB_END(setBoundingRect,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

/*!
 * Waits for the controller to be quiet.
 * @return kIOReturnSuccess if quiet. kIOReturnOffline if terminating.
 *   kIOReturnTimeout (and possibly others) if waitQuiet() times out.
 */
IOReturn IOFramebuffer::waitQuietController()
{
    IOFB_START(waitQuietController,0,0,0);
    IOReturn status = kIOReturnOffline;
    IOService *pci = this;
    const uint64_t  timeout = 15ULL;
    uint64_t    pciRegID = 0;

    if (kIOGDbgNoWaitQuietController & gIOGDebugFlags)
    {
        D(GENERAL, thisName, " disabled\n");
        IOFB_END(waitQuietController,0,0,__LINE__);
        return kIOReturnSuccess;
    }

    if (isInactive()) goto done;

    // Only wait if controller has never opened. Something is busy long
    // enough to time out after first FB opens in rdar://31310949.
    if (__private->controller && !(kIOFBNotOpened & __private->controller->fState))
    {
        D(GENERAL, thisName, " already waited\n");
        IOFB_END(waitQuietController,0,0,__LINE__);
        return kIOReturnSuccess;
    }

    for (; pci && !OSDynamicCast(IOPCIDevice, pci); pci = pci->getProvider())
    {
        // Do nothing while we look for an ancestral IOPCIDevice.
    }
    if (OSDynamicCast(IOPCIDevice, pci)) {
        pciRegID = pci->getRegistryEntryID();
        if (pci->isInactive()) goto done;
        if ((status = pci->waitQuiet(timeout*1000*1000*1000 /* ns */))) goto done;
        status = kIOReturnOffline;
        if (pci->isInactive()) goto done;
    }

    if (isInactive()) goto done;

    status = kIOReturnSuccess;

done:
    IOG_KTRACE(DBG_IOG_WAIT_QUIET, DBG_FUNC_NONE,
               0, pciRegID,
               0, status,
               0, getRegistryEntryID(),
               0, timeout);
    D(GENERAL, thisName, " time left %llu, status %d, regId %#llx\n",
        timeout, status, getRegistryEntryID());
    IOFB_END(waitQuietController,status,0,0);
    return status;
}

void IOFramebuffer::setPlatformConsole(PE_Video *consoleInfo, const unsigned op,
                                       const uint64_t where)
{
    IOG_KTRACE(DBG_IOG_PLATFORM_CONSOLE, DBG_FUNC_NONE,
               0, where, 0, __private->regID,
               0, static_cast<bool>(consoleInfo), 0, op);
    getPlatform()->setConsoleInfo(consoleInfo, op);
    if (consoleInfo)
        IOG_KTRACE(DBG_IOG_CONSOLE_CONFIG, DBG_FUNC_NONE,
                   0, __private->regID,
                   0, consoleInfo->v_width << 32 | consoleInfo->v_height,
                   0, consoleInfo->v_rowBytes,
                   0, consoleInfo->v_depth << 32 | consoleInfo->v_scale);
    if (consoleInfo && kPEEnableScreen == op) {
        if (!fVramMap) {
            DEBG1(thisName, " BUG %lld setConsoleInfo null vram %p",
                  where, fFrameBuffer);
            return;
        }
        const uint64_t requiredLen
            = consoleInfo->v_rowBytes * consoleInfo->v_height;
        const uint64_t mapLen = fVramMap->getLength();
        if (mapLen < requiredLen) {
            DEBG1(thisName, " BUG %lld setConsoleInfo %lld < %lld\n",
                  where, mapLen, requiredLen);
            panic("fFramebuffer is too small for this console spec");
        }
    }
}

/**
 ** IOUserClient methods
 **/

IOReturn IOFramebuffer::newUserClient(  task_t          owningTask,
                                        void *          security_id,
                                        UInt32          type,
                                        IOUserClient ** clientH )
{
    IOFB_START(newUserClient,type,0,0);
    IOReturn            err = kIOReturnSuccess;
    IOUserClient *      newConnect = 0;
    IOUserClient *      theConnect = 0;

    // Don't allow clients to attach while matching/termination are in progress.
    if ((err = waitQuietController())) {
        DEBG(thisName, " waitQuietController %#x\n", err);
        IOG_KTRACE(DBG_IOG_NEW_USER_CLIENT, DBG_FUNC_NONE,
                   0, __private->regID, 0, type, 0, err,
                   0, /* location */ 2);
        IOFB_END(newUserClient,err,__LINE__,0);
        IOLog("%s: newUserClient waitQuietController failed: 0x%08x %#llx %s\n",
              thisName, err, getRegistryEntryID(),
              getMetaClass()->getClassName());
        return err;
    }

    SYSGATEGUARD(sysgated);

    switch (type)
    {
        case kIOFBServerConnectType:
            if (fServerConnect)
                err = kIOReturnExclusiveAccess;
            else
            {
                if ((this == gIOFBConsoleFramebuffer) || !gIOFBConsoleFramebuffer)
                    setPlatformConsole(0, kPEReleaseScreen,
                                       DBG_IOG_SOURCE_NEWUSERCLIENT);

                err = open();
                if (kIOReturnSuccess == err)
                    newConnect = IOFramebufferUserClient::withTask(owningTask);
                if (!newConnect)
                    err = kIOReturnNoResources;
            }
            break;

        case kIOFBSharedConnectType:
            if (fSharedConnect)
            {
                DEBG(thisName, " existing shared\n");
                theConnect = fSharedConnect;
                theConnect->retain();
            }
            else if (fServerConnect)
            {
                DEBG(thisName, " new shared\n");
                newConnect = IOFramebufferSharedUserClient::withTask(owningTask);
                if (!newConnect)
                    err = kIOReturnNoResources;
            }
            else
                err = kIOReturnNotOpen;
            break;

        default:
            err = kIOReturnBadArgument;
    }

    if (newConnect)
    {
        FBGATEGUARD(ctrlgated, this);

        if ((false == newConnect->attach(this)) ||
            (false == newConnect->start(this)))
        {
            DEBG(thisName, " IOGNUC new shared failed\n");

            newConnect->detach( this );
            OSSafeReleaseNULL(newConnect);
            err = kIOReturnNotAttached;
        }
        else
            theConnect = newConnect;
    }

    *clientH = theConnect;
    IOG_KTRACE(DBG_IOG_NEW_USER_CLIENT, DBG_FUNC_NONE,
               0, __private->regID, 0, type, 0, err,
               0, /* location */ 0);
    IOFB_END(newUserClient,err,__LINE__,0);
    if (err || !theConnect)
        IOLog("%s: newUserClient failed: %d 0x%08x %#llx %s\n",
              thisName, !!theConnect, err, getRegistryEntryID(),
              getMetaClass()->getClassName());
    return (err);
}

IOReturn IOFramebuffer::extGetDisplayModeCount(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extGetDisplayModeCount,0,0,0);
    IOFramebuffer * inst  = (IOFramebuffer *) target;
    uint64_t *      count = &args->scalarOutput[0];

    IOReturn err;

    if ((err = inst->extEntry(false, kIOGReportAPIState_GetDisplayModeCount)))
    {
        IOFB_END(extGetDisplayModeCount,err,__LINE__,0);
        return (err);
    }

    FB_START(getDisplayModeCount,0,__LINE__,0);
	*count = inst->dead ? 0 : inst->getDisplayModeCount();
    FB_END(getDisplayModeCount,0,__LINE__,0);

    inst->extExit(err, kIOGReportAPIState_GetDisplayModeCount);

    IOFB_END(extGetDisplayModeCount,err,0,0);
    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::extGetDisplayModes(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extGetDisplayModes,0,0,0);
    IOFramebuffer *   inst     = (IOFramebuffer *) target;
    IODisplayModeID * allModes = (IODisplayModeID *) args->structureOutput;
    uint32_t *        size     = &args->structureOutputSize;

    IOReturn    err;
    IOByteCount outSize;

    if ((err = inst->extEntry(false, kIOGReportAPIState_GetDisplayModes)))
    {
        IOFB_END(extGetDisplayModes,err,__LINE__,0);
        return (err);
    }

    FB_START(getDisplayModeCount,0,__LINE__,0);
    outSize = inst->getDisplayModeCount() * sizeof( IODisplayModeID);
    FB_END(getDisplayModeCount,0,__LINE__,0);
	if (*size >= outSize)
	{
		*size = static_cast<uint32_t>(outSize);
        FB_START(getDisplayModes,0,__LINE__,0);
		err = inst->getDisplayModes( allModes );
        FB_END(getDisplayModes,err,__LINE__,0);
	}
	else
		err = kIOReturnBadArgument;

    inst->extExit(err, kIOGReportAPIState_GetDisplayModes);

    IOFB_END(extGetDisplayModes,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::extGetVRAMMapOffset(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extGetVRAMMapOffset,0,0,0);
    IOFramebuffer * inst   = (IOFramebuffer *) target;
    // IOPixelAperture aperture = args->scalarInput[0];
    uint64_t *      offset = &args->scalarOutput[0];

    IOReturn err;
    
    if ((err = inst->extEntry(false, kIOGReportAPIState_GetVRAMMapOffset)))
    {
        IOFB_END(extGetVRAMMapOffset,err,__LINE__,0);
        return (err);
    }

    if (inst->fVramMap) *offset = inst->fVramMapOffset;
	else
    {
    	*offset = 0;
    	err = kIOReturnNoSpace;
    }

    inst->extExit(err, kIOGReportAPIState_GetVRAMMapOffset);

    IOFB_END(extGetVRAMMapOffset,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::_extEntry(bool system, bool allowOffline, uint32_t apibit, const char * where)
{
    IOFB_START(_extEntry,system,allowOffline,apibit);
    IOReturn             err = kIOReturnSuccess;

    if (!__private->controller) return kIOReturnNotReady;

	if (system)
	{
		gIOFBSystemWorkLoop->timedCloseGate(thisName, where);
	}

    FBWL(this)->timedCloseGate(thisName, where);

    while (!err && !pagingState && !gIOFBSystemPowerAckTo)
    {
        if (isInactive()) {
            err = kIOReturnOffline;
            break;
        }

//		IODisplayWrangler::activityChange(this);
		if (system)
		{
			FBUNLOCK(this);
			err = gIOFBSystemWorkLoop->sleepGate(&fServerConnect, THREAD_UNINT);
			FBLOCK(this);
		}
		else
			err = FBWL(this)->sleepGate(&fServerConnect, THREAD_UNINT);

        if (kIOReturnSuccess != err)
            break;
    }

	if ((kIOReturnSuccess == err) && !__private->online && !allowOffline) err = kIOReturnOffline;

	if (kIOReturnSuccess != err) DEBG1(thisName, " %s: err 0x%x\n", where, err);

	if (kIOReturnSuccess != err)
		_extExit(system, err, 0, where);
    else
        API_ATTRIB_SET(this, apibit);

    IOFB_END(_extEntry,err,0,0);
    return (err);
}

void IOFramebuffer::_extExit(bool system, IOReturn result, uint32_t apibit, const char * where)
{
    IOFB_START(_extExit,system,apibit,0);
    API_ATTRIB_CLR(this, apibit);
	FBUNLOCK(this);
	if (system)
		SYSUNLOCK();
    IOFB_END(_extExit,0,0,0);
}

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::extSetBounds(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extSetBounds,0,0,0);
    IOFramebuffer * inst   = (IOFramebuffer *) target;
    IOGBounds *     bounds = (IOGBounds *) args->structureInput;
    uint32_t		inSize = args->structureInputSize;
    uint32_t        virtIdx = 0;
    SInt16			ox, oy;

    IOReturn       err;
    StdFBShmem_t * shmem;

    if (inSize < sizeof(IOGBounds))
    {
        IOFB_END(extSetBounds,kIOReturnBadArgument,0,0);
	    return (kIOReturnBadArgument);
    }
	if (inSize >= (2 * sizeof(IOGBounds)))
		virtIdx = 1;

	DEBG1(inst->thisName, " (%d, %d), (%d, %d)\n",
                bounds->minx, bounds->miny,
                bounds->maxx, bounds->maxy);

    if (true &&
        (1 == (bounds->maxx - bounds->minx))
        && (1 == (bounds->maxy - bounds->miny)))
    {
        shmem = GetShmem(inst);
        if (shmem)
        {
			ox = shmem->screenBounds.minx;
			oy = shmem->screenBounds.miny;

            shmem->screenBounds = *bounds;
			inst->__private->screenBounds[0] = bounds[0];
			inst->__private->screenBounds[1] = bounds[virtIdx];

			ox = bounds->minx - ox;
			oy = bounds->miny - oy;
			shmem->saveRect.minx += ox;
			shmem->saveRect.maxx += ox;
			shmem->saveRect.miny += oy;
			shmem->saveRect.maxy += oy;
    	}

        IOFB_END(extSetBounds,kIOReturnSuccess,__LINE__,0);
        return (kIOReturnSuccess);
    }
    
    if ((err = inst->extEntry(true, kIOGReportAPIState_SetBounds)))
    {
        IOFB_END(extSetBounds,err,__LINE__,0);
        return (err);
    }

	shmem = GetShmem(inst);
    if (shmem)
    {
    	ox = shmem->screenBounds.minx;
    	oy = shmem->screenBounds.miny;
    	
        if ((kIOFBHardwareCursorActive == shmem->hardwareCursorActive) && inst->__private->online)
        {
            IOGPoint * hs;
            hs = &shmem->hotSpot[0 != shmem->frame];
            (void)inst->_setCursorState(
                      shmem->cursorLoc.x - hs->x - ox,
                      shmem->cursorLoc.y - hs->y - oy, false );
        }

        shmem->screenBounds = *bounds;
		inst->__private->screenBounds[0] = bounds[0];
		inst->__private->screenBounds[1] = bounds[virtIdx];

		ox = bounds->minx - ox;
		oy = bounds->miny - oy;
		shmem->saveRect.minx += ox;
		shmem->saveRect.maxx += ox;
		shmem->saveRect.miny += oy;
		shmem->saveRect.maxy += oy;
    }

    inst->extExit(err, kIOGReportAPIState_SetBounds);

    IOFB_END(extSetBounds,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::extValidateDetailedTiming(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extValidateDetailedTiming,0,0,0);
    IOFramebuffer * inst           = (IOFramebuffer *) target;
    void *          description    = const_cast<void *>(args->structureInput);
    void *          outDescription = args->structureOutput;
    uint32_t        inSize         = args->structureInputSize;
    uint32_t *      outSize        = &args->structureOutputSize;

    IOReturn    err;

    if (*outSize != inSize)
    {
        IOFB_END(extValidateDetailedTiming,kIOReturnBadArgument,0,0);
        return (kIOReturnBadArgument);
    }
    if ((err = inst->extEntry(false, kIOGReportAPIState_ValidateDetailedTiming)))
    {
        IOFB_END(extValidateDetailedTiming,err,__LINE__,0);
        return (err);
    }

    FB_START(validateDetailedTiming,0,__LINE__,0);
	err = inst->validateDetailedTiming( description, inSize );
    FB_END(validateDetailedTiming,err,__LINE__,0);

    if (kIOReturnSuccess == err)
        bcopy( description, outDescription, inSize );

    inst->extExit(err, kIOGReportAPIState_ValidateDetailedTiming);

    IOFB_END(extValidateDetailedTiming,err,0,0);
    return (err);
}


IOReturn IOFramebuffer::extSetColorConvertTable(
        OSObject * /*target*/, void * /*reference*/, IOExternalMethodArguments * /*args*/)
{
    // This functionality is not in use by CoreDisplay and now officially unsupported.
    // <rdar://problem/30655818> IOFramebuffer::extSetColorConvertTable potential out of bounds read
    return (kIOReturnUnsupported);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFramebuffer::requestTerminate(IOService * provider, IOOptionBits options)
{
    IOFB_START(requestTerminate,options,0,0);
    DEBG1(thisName, "(%p, %p, %#x)\n", this, provider, (uint32_t)options);
    bool b = super::requestTerminate(provider, options);
    IOFB_END(requestTerminate,b,0,0);
    return (b);
}

bool IOFramebuffer::terminate(IOOptionBits options)
{
    IOFB_START(terminate,options,0,0);
    bool status = super::terminate(options);
    SYSGATEGUARD(sysgated);
    FBGATEGUARD(ctrlgated, this);
    DEBG1(thisName, "(%p, %#x)\n", this, (uint32_t)options);
    deliverFramebufferNotification(kIOFBNotifyTerminated);

    // wakeup is not gate specific
    FBWL(this)->wakeupGate(&fServerConnect, kManyThreadWakeup);

    IOFB_END(terminate,status,0,0);
    return status;
}

bool IOFramebuffer::willTerminate(IOService * provider, IOOptionBits options)
{
    IOFB_START(willTerminate,options,0,0);
    DEBG1(thisName, "(%p, %p, %#x)\n", this, provider, (uint32_t)options);
    bool b = super::willTerminate(provider, options);
    IOFB_END(willTerminate,b,0,0);
    return (b);
}

bool IOFramebuffer::didTerminate(IOService * provider, IOOptionBits options, bool * defer)
{
    IOFB_START(didTerminate,options,0,0);
    DEBG1(thisName, "(%p, %p, %#x)\n", this, provider, (uint32_t)options);
    bool b = super::didTerminate(provider, options, defer);
    IOFB_END(didTerminate,b,0,0);
    return (b);
}

void IOFramebuffer::stop(IOService * provider)
{
    IOFB_START(stop,0,0,0);
    unsigned i;
    SYSGATEGUARD(sysgated);
    FBGATEGUARD(ctrlgated, this);

#define UNREGISTER_INTERRUPT(x)                                                \
do {                                                                           \
    if (x) {                                                                   \
        unregisterInterrupt(x);                                                \
        x = nullptr;                                                           \
    }                                                                          \
} while (false)

#define RELEASE_EVENT_SOURCE(wl, x)                                            \
do {                                                                           \
    if (x) {                                                                   \
        (wl)->removeEventSource(x);                                            \
        OSSafeReleaseNULL(x);                                                  \
    }                                                                          \
} while (false)

    DEBG1(thisName, "(%p, %p)\n", this, provider);

    setAttribute(kIOFBStop, 0);

    if (__private->controller) __private->controller->unhookFB(this);

    if (opened)
    {
        dead = true;

        UNREGISTER_INTERRUPT(__private->vblInterrupt);
        UNREGISTER_INTERRUPT(__private->connectInterrupt);
        UNREGISTER_INTERRUPT(__private->dpInterruptRef);

        RELEASE_EVENT_SOURCE(FBWL(this), __private->deferredVBLDisableEvent);
        RELEASE_EVENT_SOURCE(FBWL(this), __private->vblUpdateTimer);
        RELEASE_EVENT_SOURCE(FBWL(this), __private->deferredCLUTSetEvent);
        RELEASE_EVENT_SOURCE(FBWL(this), __private->deferredCLUTSetTimerEvent);
        RELEASE_EVENT_SOURCE(FBWL(this), __private->dpInterruptES);
        RELEASE_EVENT_SOURCE(FBWL(this), __private->deferredSpeedChangeEvent);

        OSSafeReleaseNULL(__private->pmSettingNotificationHandle);
        OSSafeReleaseNULL(__private->paramHandler);
        priv = nullptr;
        OSSafeReleaseNULL(sharedCursor);

        // Disable notifications so we aren't delivering notifications for
        // FBs that have stopped. Holders of these notifiers should remove
        // them via IONotifier::remove().
        // Disable all notification callouts
        disableNotifiers();

        i = gAllFramebuffers->getNextIndexOfObject(this, 0);
        if ((unsigned) -1 != i) gAllFramebuffers->removeObject(i);

        gIOFBOpenGLMask &= ~__private->openGLIndex;

        FB_START(setAttribute, kIOSystemPowerAttribute,
                 __LINE__, kIOMessageSystemWillPowerOff);
        setAttribute(kIOSystemPowerAttribute, kIOMessageSystemWillPowerOff);
        FB_END(setAttribute,0,__LINE__,0);

        IOG_KTRACE(DBG_IOG_CLAMP_POWER_ON, DBG_FUNC_NONE,
                   0, DBG_IOG_SOURCE_STOP, 0, 0, 0, 0, 0, 0);

        temporaryPowerClampOn();        // only to clear out kIOPMPreventSystemSleep
        PMstop();
    }
    else
    {
        i = gStartedFramebuffers->getNextIndexOfObject(this, 0);
        if ((unsigned) -1 != i) gStartedFramebuffers->removeObject(i);
    }

    RELEASE_EVENT_SOURCE(gIOFBSystemWorkLoop, __private->fCloseWorkES);

    if (this == gIOFBConsoleFramebuffer) {
        gIOFBConsoleFramebuffer = NULL;
        findConsole();
    }

    if (!gIOFBBacklightDisplayCount) {
        D(GENERAL, thisName, " no displays; probe\n");
        triggerEvent(kIOFBEventProbeAll);
    }
    DEBG(thisName, " opened %d, started %d, gl 0x%08x\n",
        gAllFramebuffers->getCount(), gStartedFramebuffers->getCount(),
        gIOFBOpenGLMask);

#undef UNREGISTER_INTERRUPT
#undef RELEASE_EVENT_SOURCE

    super::stop(provider);
    IOFB_END(stop,0,0,0);
}

void IOFramebuffer::free()
{
    IOFB_START(free,0,0,0);
    if (vblSemaphore)
        semaphore_destroy(kernel_task, vblSemaphore);
    if (thisName && thisNameLen) {
        // thisName starts off as a static string "IOFB?"; thisNameLen = 0 then.
        // Later, thisName is allocated on the heap to contain a unique value;
        // thisNameLen tracks the size of the heap allocation, if it exists.
        void *nameAllocation = (void *)thisName;
        IODelete(nameAllocation, char, thisNameLen);
        thisName = NULL;
    }
    OSSafeReleaseNULL(userAccessRanges);
    if (serverMsg) {
        IODelete(serverMsg, mach_msg_header_t, 1);
        serverMsg = NULL;
    }
    if (__private)
    {
#define SAFE_IODELETE(x, t, l)                                                 \
do {                                                                           \
    if (x && l) {                                                              \
        IODelete(x, t, l);                                                     \
        x = NULL;                                                              \
        l = 0;                                                                 \
    }                                                                          \
} while(0)
        SAFE_IODELETE(__private->gammaData, UInt8, __private->gammaDataLen);
        SAFE_IODELETE(__private->rawGammaData, UInt8, __private->rawGammaDataLen);
        SAFE_IODELETE(__private->clutData, UInt8, __private->clutDataLen);
        SAFE_IODELETE(__private->hibernateGammaData, uint8_t, __private->hibernateGammaDataLen);
        SAFE_IODELETE(__private->cursorFlags, UInt8, __private->numCursorFrames);
        SAFE_IODELETE(__private->cursorImages, volatile unsigned char *, __private->numCursorFrames);
        SAFE_IODELETE(__private->cursorMasks, volatile unsigned char *, __private->numCursorFrames);

        OSSafeReleaseNULL(__private->controller);
        IODelete(__private, IOFramebufferPrivate, 1 );
        __private = 0;
    }
    super::free();
    IOFB_END(free,0,0,0);
}

/*!
 * Notification to IOAccelerator to initialize display machinery.
 *
 * In order for GPUWrangler to provide IOAccelIndex values before IOFB::open(),
 * the GPUWrangler now triggers a probe via message() once the "anchor node"
 * is quiet (after everything under the IOPCIDevice has finished start()).
 * (IOAccelIndex is published by IOAF when it receives requestProbe().)
 *
 * Note for IOFB's without a supported GPUWrangler "anchor node" (IOPCIDevice or
 * AGDC nub), probeAccelerator() still happens after all the IOFramebuffers
 * have started, early in the first IOFB's open() (before enableController(),
 * and before the first modeWillChange/modeDidChange notifications on this
 * GPU's IOFB's).
 */
IOReturn IOFramebuffer::probeAccelerator()
{
    IOReturn status = kIOReturnNotFound;

    IOFB_START(probeAccelerator,0,0,0);

    if (__private && __private->controller) {
        // Only probe once per controller.
        if (kIOFBAccelProbed & __private->controller->fState)
        {
            IOFB_END(probeAccelerator,0,__LINE__,0);
            return kIOReturnSuccess;
        }
        __private->controller->setState(kIOFBAccelProbed);
    }

    IOService *pci = this;
    while ((pci = pci->getProvider()))
    {
        if (OSDynamicCast(IOPCIDevice, pci))
            break;
    }
    if (!pci)
    {
        IOFB_END(probeAccelerator,-1,__LINE__,0);
        return kIOReturnNoDevice;
    }

    OSIterator *descendantIter = IORegistryIterator::iterateOver(
        pci, gIOServicePlane, kIORegistryIterateRecursively);
    IORegistryEntry *descendant;
    while ((descendant = OSDynamicCast(IORegistryEntry, descendantIter->getNextObject())))
    {
        if (descendant->metaCast("IOAccelerator"))
        {
            IOService *accel = OSDynamicCast(IOService, descendant);
            status = accel->requestProbe(kIOFBUserRequestProbe);
            break;
        }
    }
    OSSafeReleaseNULL(descendantIter);

    IOFB_END(probeAccelerator,status,0,0);
    return status;
}


void IOFramebuffer::initialize()
{
    IOFB_START(initialize,0,0,0);
	OSDictionary  *     matching;

	gIOFBServerInit      = true;
#if defined(__arm64__) || defined(__aarch64__)
	/* ARM boot_args replaced the x86 flags field with bootFlags. The only
	 * equivalent currently defined by the ARM ABI is dark boot. */
	uint64_t armBootFlags = ((boot_args *) PE_state.bootArgs)->bootFlags;
	gIOFBBlackBoot       = (0 != (kBootFlagsDarkBoot & armBootFlags));
	gIOFBBlackBootTheme  = 0;
#else
	gIOFBBlackBoot       = (0 != (kBootArgsFlagBlack & ((boot_args *) PE_state.bootArgs)->flags));
	gIOFBBlackBootTheme  = (0 != (kBootArgsFlagBlackBg & ((boot_args *) PE_state.bootArgs)->flags));
#endif
	if (gIOFBBlackBoot || gIOFBBlackBootTheme) gIOFBGrayValue = 0;
    gIOFBVerboseBoot     = PE_parse_boot_argn("-v", NULL,0);


	gIOFBGetSensorValueKey = OSSymbol::withCStringNoCopy(kIOFBGetSensorValueKey);
	gIOFBRotateKey = OSSymbol::withCStringNoCopy(kIOFBRotatePrefsKey);
	gIOFBStartupModeTimingKey = OSSymbol::withCStringNoCopy(kIOFBStartupTimingPrefsKey);
	gIOFBPMSettingDisplaySleepUsesDimKey = OSSymbol::withCString(kIOPMSettingDisplaySleepUsesDimKey);

	gIOFBConfigKey = OSSymbol::withCString(kIOFBConfigKey);
	gIOFBModesKey  = OSSymbol::withCString(kIOFBModesKey);
	gIOFBModeIDKey = OSSymbol::withCString(kIOFBModeIDKey);
	gIOFBModeDMKey = OSSymbol::withCString(kIOFBModeDMKey);

	matching = serviceMatching("AppleGraphicsControl");
	if ((gIOGraphicsControl = copyMatchingService(matching)))
	{
		gIOFBGCNotifier = gIOGraphicsControl->registerInterest(
									gIOGeneralInterest, &agcMessage, 0, 0 );
	}
	if (matching) matching->release();

	if (gIOFBPrefs)
		gIOFBPrefsSerializer = OSSerializer::forTarget(gIOFBPrefs, 
														&IOFramebufferLockedSerialize, 0);

	clock_interval_to_absolutetime_interval(20, kMillisecondScale, &gIOFBMaxVBLDelta);

	gIOFBRootNotifier = getPMRootDomain()->registerInterest(
							gIOPriorityPowerStateInterest, &systemPowerChange, 0, 0 );
	gIOFBClamshellCallout = thread_call_allocate(&delayedEvent, (thread_call_param_t) 0);
	static uint32_t zero = 0;
	static uint32_t one = 1;
	gIOFBZero32Data = OSData::withBytesNoCopy(&zero, sizeof(zero));
	gIOFBOne32Data = OSData::withBytesNoCopy(&one, sizeof(one));
	gIOFBGray32Data = OSData::withBytesNoCopy(&gIOFBGrayValue, sizeof(gIOFBGrayValue));

	gIOGraphicsPrefsVersionKey = OSSymbol::withCStringNoCopy(kIOGraphicsPrefsVersionKey);
	gIOGraphicsPrefsVersionValue = OSNumber::withNumber(kIOGraphicsPrefsCurrentVersion, 32);
	gIOFBVblDeltaMult = (1 << 16);

    matching  = nameMatching("IOHIDSystem");
	IOService * hidsystem = copyMatchingService(matching);
	if (hidsystem)
	{
		gIOFBHIDWorkLoop = hidsystem->getWorkLoop();
		if (gIOFBHIDWorkLoop) gIOFBHIDWorkLoop->retain();
		hidsystem->release();
	}
	if (matching) matching->release();

	// system work
	gIOFBWorkES = IOInterruptEventSource::interruptEventSource(gIOFBSystemWorkLoop, &systemWork);
	if (gIOFBWorkES)
		gIOFBSystemWorkLoop->addEventSource(gIOFBWorkES);

	gIOFBDelayedPrefsEvent = IOTimerEventSource::timerEventSource(
											gAllFramebuffers, &writePrefs);
	if (gIOFBDelayedPrefsEvent)
		gIOFBSystemWorkLoop->addEventSource(gIOFBDelayedPrefsEvent);

	gIOFBServerAckTimer = IOTimerEventSource::
            timerEventSource(gAllFramebuffers, &serverPowerTimeout);
	if (gIOFBServerAckTimer)
		gIOFBSystemWorkLoop->addEventSource(gIOFBServerAckTimer);


    // <rdar://problem/30812452> IOGraphics: Publish hibernate images memory descriptor on MUX systems
    // Default to always setting the preview image and only disable if legacy MUX behavior is desired via
    // NVRAM property
    gIOFBSetPreviewImage = (kIOGDbgForceLegacyMUXPreviewPolicy & gIOGDebugFlags) ? false : true;
    DEBG("FB", "gIOFBSetPreviewImage: %s\n", gIOFBSetPreviewImage ? "true" : "false");

    IOFB_END(initialize,0,0,0);
}

// flag must be one of IOFBController::CopyType
IOFBController *IOFramebuffer::copyController(const int controllerCreateFlag)
{
    IOFB_START(copyController,controllerCreateFlag,0,0);
    IOFBController  * fbc = NULL;
    if (__private && __private->controller)
    {
        fbc = __private->controller->copy();
        IOFB_END(copyController,0,0,0);
        return (fbc);
    }

    assert(IOFBController::kLookupOnly  == controllerCreateFlag
        || IOFBController::kDepIDCreate == controllerCreateFlag
        || IOFBController::kForceCreate == controllerCreateFlag);

    const IOFBController::CopyType flag
        = static_cast<IOFBController::CopyType>(controllerCreateFlag);

    fbc = IOFBController::copyController(this, flag);
    IOFB_END(copyController,0,1,0);
    return (fbc);
}

bool IOFramebuffer::attach(IOService *provider)
{
    IOFB_START(attach,0,0,0);
    bool foundAGDC = false;
    bool tunnelled = (NULL != provider->getProperty(kIOPCITunnelledKey, gIOServicePlane,
            kIORegistryIterateRecursively | kIORegistryIterateParents));
    bool status = false;

    if (tunnelled) do {
        IOService *pci = provider;
        while ((NULL == OSDynamicCast(IOPCIDevice, pci)) &&
               (NULL != (pci = pci->getProvider())))
        {
            // Searching for device.
        }

        if (!pci) break;

        OSIterator *descendantIter = IORegistryIterator::iterateOver(
            pci, gIOServicePlane, kIORegistryIterateRecursively);
        IORegistryEntry *descendant;
        while ((descendant = OSDynamicCast(IORegistryEntry, descendantIter->getNextObject())))
        {
            if (descendant->metaCast("AppleGraphicsDeviceControl"))
            {
                foundAGDC = true;
                break;
            }
        }
        OSSafeReleaseNULL(descendantIter);
    } while (false);

    if (!tunnelled || foundAGDC) {
        status = super::attach(provider);
    }

    IOFB_END(attach,status,0,0);
    return status;
}

bool IOFramebuffer::start( IOService * provider )
{
    IOFB_START(start,0,0,0);

    IOGraphicsDelayKnob("fbstart");
    IOG_KTRACE_LOG_SYNCH(DBG_IOG_LOG_SYNCH);

    if (!super::start(provider))
    {
        IOFB_END(start,false,__LINE__,0);
        return (false);
    }

    // Cache the 'chosen' RegistryEntry in the DeviceTree plane
    if (!gChosenEntry)
        gChosenEntry = IORegistryEntry::fromPath("/chosen", gIODTPlane);

    if (!__private)
    {
        __private = IONew( IOFramebufferPrivate, 1 );
        if (!__private)
        {
            IOFB_END(start,false,__LINE__,0);
            return (false);
        }
        bzero( __private, sizeof(IOFramebufferPrivate) );
        __private->lastNotifyOnline = 0xdd;
        __private->regID            = getRegistryEntryID();

        userAccessRanges = OSArray::withCapacity( 1 );
        if (!userAccessRanges)
        {
            IOFB_END(start,false,__LINE__,0);
            return (false);
        }

        serverMsg = IONew(mach_msg_header_t, 1);
        if (!serverMsg)
        {
            IOFB_END(start,false,__LINE__,0);
            return (false);
        }
        bzero(serverMsg, sizeof(mach_msg_header_t));

        __private->fCloseWorkES = IOInterruptEventSource::interruptEventSource(this,
            OSMemberFunctionCast(IOInterruptEventSource::Action, this,
                &IOFramebuffer::closeWork));
        if (!static_cast<bool>(__private->fCloseWorkES)) return false;
        gIOFBSystemWorkLoop->addEventSource(__private->fCloseWorkES);

        // Grab the gate before we try to find a controller
        SYSGATEGUARD(sysgated);
        __private->controller = copyController(IOFBController::kDepIDCreate);
    }
    if (!thisName)
    	thisName = "IOFB?";

#if RLOG
    {
        OSNumber *depID = OSDynamicCast(OSNumber, getProperty(kIOFBDependentIDKey));
        if (depID)
        {
            DEBG(thisName, " %llx Starting depID=%llx controller %p\n",
                 __private->regID, depID->unsigned64BitValue(),
                 OBFUSCATE(__private->controller));
        }
        else
            DEBG(thisName, " %llx Starting no depID\n", __private->regID);
    }
#endif // RLOG

    // initialize superclass power management variables
    PMinit();
    // attach into the power management hierarchy
    setProperty("IOPMStrictTreeOrder", kOSBooleanTrue);
    provider->joinPMtree(this);
    temporaryPowerClampOn();

    {
        SYSGATEGUARD(sysgated);

        do
        {
            const int       kBufLen = 1024;
            IOFramebuffer * fb;
            char          * pathThis;
            char          * path;
            int             pathLen;
            unsigned        i;

            pathLen = kBufLen;
            pathThis = IONew(char, 2 * kBufLen);
            if (!pathThis) continue;
            path = &pathThis[pathLen];

            if (!getPath(pathThis, &pathLen, gIOServicePlane)) pathThis[0] = 0;
            for (i = 0;
                (fb = (IOFramebuffer *) gStartedFramebuffers->getObject(i));
                i++)
            {
                pathLen = kBufLen;
                if (!fb->getPath(path, &pathLen, gIOServicePlane)) path[0] = 0;
                // Perform an insertion O(n^2) sort using path. Is the
                // optimisation worth it?
                if (0 > strcmp(pathThis, path)) break;
            }
            gStartedFramebuffers->setObject(i, this);

            IODelete(pathThis, char, 2 * kBufLen);
        }
        while (false);
    }

    registerService();

    IOFB_END(start,true,0,0);
    return (true);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//
// BEGIN:       Implementation of the evScreen protocol
//

void IOFramebuffer::deferredMoveCursor( IOFramebuffer * inst )
{
    IOFB_START(deferredMoveCursor,0,0,0);
    StdFBShmem_t *      shmem = GetShmem(inst);
    IOReturn            err = kIOReturnSuccess;

    if (shmem->hardwareCursorActive)
    {
        if (shmem->cursorObscured)
        {
            shmem->cursorObscured = 0;
            if (shmem->cursorShow)
                --shmem->cursorShow;
        }
        if (shmem->hardwareCursorShields && shmem->shieldFlag)
            CheckShield(inst);
        if (!shmem->cursorShow)
        {
            IOGPoint * hs;
            hs = &shmem->hotSpot[0 != shmem->frame];
            err = inst->_setCursorState(
                        shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx,
                        shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny, true );
#if 0
			// debug
			shmem->cursorRect.minx = shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx;
			shmem->cursorRect.miny = shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny;
			shmem->cursorRect.maxx = 0;
			shmem->cursorRect.maxy = 0;
#endif
        }
    }
    else
    {
        if (!shmem->cursorShow++)
            RemoveCursor(inst);
        if (shmem->cursorObscured)
        {
            shmem->cursorObscured = 0;
            if (shmem->cursorShow)
                --shmem->cursorShow;
        }
        if (shmem->shieldFlag)
            CheckShield(inst);
        if (shmem->cursorShow)
            if (!--shmem->cursorShow)
                DisplayCursor(inst);

        FB_START(flushCursor,0,__LINE__,0);
        inst->flushCursor();
        FB_END(flushCursor,0,__LINE__,0);

        if (inst->__private->cursorPanning)
        {
            IOGPoint * hs;
            hs = &shmem->hotSpot[0 != shmem->frame];
            FB_START(setCursorState,0,__LINE__,0);
            err = inst->setCursorState(
                      shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx,
                      shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny, false );
            FB_END(setCursorState,err,__LINE__,0);
        }
    }
    IOFB_END(deferredMoveCursor,0,0,0);
}

void IOFramebuffer::cursorWork( OSObject * p0, IOInterruptEventSource * evtSrc, int intCount )
{
    IOFB_START(cursorWork,intCount,0,0);
    IOFramebuffer *             inst = (IOFramebuffer *) p0;
    StdFBShmem_t *              shmem = GetShmem(inst);
    struct IOFramebufferPrivate * __private = inst->__private;
    IOFBCursorControlAttribute  * cursorControl = &__private->cursorControl;
    IOReturn                    ret;
    IOHardwareCursorDescriptor  desc;

    IOOptionBits todo = inst->__private->cursorToDo;

    while (todo)
    {
        if (2 & todo)
        {
            desc.majorVersion   = kHardwareCursorDescriptorMajorVersion;
            desc.minorVersion   = kHardwareCursorDescriptorMinorVersion;
            desc.height         = shmem->cursorSize[0 != __private->framePending].height;
            desc.width          = shmem->cursorSize[0 != __private->framePending].width;
            desc.bitDepth       = inst->__private->cursorBytesPerPixel * 8;
            desc.maskBitDepth   = 0;
            desc.colorEncodings = 0;
            desc.flags          = 0;
            desc.supportedSpecialEncodings = kTransparentEncodedPixel;

            ret = (*cursorControl->callouts->setCursorImage) (
                      cursorControl->inst, cursorControl->ref,
                      &desc, (void *)(uintptr_t) __private->framePending );
        }
        if (1 & todo)
            ret = (*cursorControl->callouts->setCursorState) (
                      cursorControl->inst, cursorControl->ref,
                      __private->xPending, __private->yPending, __private->visiblePending );

        todo = __private->cursorToDo & ~todo;
        __private->cursorToDo = todo;
    }
    IOFB_END(cursorWork,0,0,0);
}

IOOptionBits IOFramebuffer::_setCursorImage( UInt32 frame )
{
    IOFB_START(_setCursorImage,frame,0,0);
    StdFBShmem_t * shmem = GetShmem(this);
    IOGPoint *     hs;
    IOOptionBits   flags;
    bool           liftCursor, hsChange;

    hs = &shmem->hotSpot[0 != frame];
    hsChange = ((hs->x != __private->lastHotSpot.x) || (hs->y != __private->lastHotSpot.y));
    __private->lastHotSpot = *hs;
// This code is never executed
//    if (false && hsChange)
//    {
//        if (GetShmem(this) && __private->deferredCLUTSetEvent && shmem->vblCount && !__private->cursorSlept && !suspended)
//        {
//            if (false)
//            {
//                __private->waitVBLEvent = hs;
//                FBWL(this)->sleepGate(hs, THREAD_UNINT);
//            }
//            else if (CMP_ABSOLUTETIME(&shmem->vblDeltaReal, &gIOFBMaxVBLDelta) < 0)
//            {
//                AbsoluteTime deadline = shmem->vblTime;
//                ADD_ABSOLUTETIME(&deadline, &shmem->vblDeltaReal);
//                clock_delay_until(deadline);
//            }
//        }
//    }

	liftCursor = ((kIOFBHardwareCursorActive != shmem->hardwareCursorActive) && !shmem->cursorShow);
    if (liftCursor)
	{
        RemoveCursor(this);
    }
    FB_START(setCursorImage,0,__LINE__,0);
    flags = (kIOReturnSuccess == setCursorImage( (void *)(uintptr_t) frame ))
            ? kIOFBHardwareCursorActive : 0;
    FB_END(setCursorImage,0,__LINE__,0);
    if (liftCursor || (hsChange && !shmem->cursorShow))
	{
        DisplayCursor(this);
	}
    if (!flags && __private->cursorThread && (__private->cursorBytesPerPixel >= 2))
    {
        __private->framePending = frame;
        __private->cursorToDo |= 2;
        flags = kIOFBHardwareCursorActive | kIOFBHardwareCursorInVRAM;
    }

    IOFB_END(_setCursorImage,flags,0,0);
    return (flags);
}

IOReturn IOFramebuffer::_setCursorState( SInt32 x, SInt32 y, bool visible )
{
    IOFB_START(_setCursorState,x,y,visible);
    StdFBShmem_t *shmem = GetShmem(this);
    IOReturn ret = kIOReturnUnsupported;

    x -= __private->cursorHotSpotAdjust[0].x;
    y -= __private->cursorHotSpotAdjust[0].y;

    if (kIOFBHardwareCursorActive == shmem->hardwareCursorActive)
    {
        FB_START(setCursorState,0,__LINE__,0);
        ret = setCursorState( x, y, visible );
        FB_END(setCursorState,ret,__LINE__,0);
    }
    else if (__private->cursorThread)
    {
        __private->cursorToDo |= 1;
        __private->xPending = x;
        __private->yPending = y;
        __private->visiblePending = visible;
    }

    IOFB_END(_setCursorState,ret,0,0);
    return (ret);
}

void IOFramebuffer::transformLocation(StdFBShmem_t * shmem,
                                        IOGPoint * cursorLoc, IOGPoint * transformLoc)
{
    IOFB_START(transformLocation,0,0,0);
    SInt32 x, y;

    x = cursorLoc->x - shmem->screenBounds.minx;
    y = cursorLoc->y - shmem->screenBounds.miny;

    if (__private->transform & kIOFBSwapAxes)
    {
        SInt32 t = x;
        x = y;
        y = t;
    }
    if (__private->transform & kIOFBInvertX)
        x = __private->framebufferWidth - x - 1;
    if (__private->transform & kIOFBInvertY)
        y = __private->framebufferHeight - y - 1;

    transformLoc->x = x + shmem->screenBounds.minx;
    transformLoc->y = y + shmem->screenBounds.miny;
    IOFB_END(transformLocation,0,0,0);
}

void IOFramebuffer::moveCursor( IOGPoint * cursorLoc, int frame )
{
    UInt32 hwCursorActive;

    if (isInactive())
        return; // rdar://30475917

    if (static_cast<UInt32>(frame) >= __private->numCursorFrames)
        return;

	nextCursorLoc = *cursorLoc;
    nextCursorFrame = frame;

	CURSORLOCK(this);

    if (frame != shmem->frame)
    {
        if (__private->cursorFlags[frame] && pagingState)
        {
            hwCursorActive = _setCursorImage( frame );
            __private->cursorFlags[frame] = hwCursorActive ? kIOFBCursorHWCapable : 0;
        }
        else
            hwCursorActive = 0;

        shmem->frame = frame;
        if (shmem->hardwareCursorActive != hwCursorActive)
        {
            SysHideCursor( this );
            shmem->hardwareCursorActive = hwCursorActive;
            if (shmem->shieldFlag
                    && ((0 == hwCursorActive) || (shmem->hardwareCursorShields)))
                CheckShield(this);
            SysShowCursor( this );
        }
    }

	if (kIOFBRotateFlags & __private->transform)
		transformLocation(shmem, &nextCursorLoc, &shmem->cursorLoc);
	else
		shmem->cursorLoc = nextCursorLoc;
	shmem->frame = frame;
	deferredMoveCursor( this );

	CURSORUNLOCK(this);
}

void IOFramebuffer::hideCursor( void )
{
    if (isInactive())
        return; // rdar://30475917

	CURSORLOCK(this);

    SysHideCursor(this);

	CURSORUNLOCK(this);
}

void IOFramebuffer::showCursor( IOGPoint * cursorLoc, int frame )
{
    UInt32 hwCursorActive;

    if (isInactive())
        return; // rdar://30475917

    if (static_cast<UInt32>(frame) >= __private->numCursorFrames)
        return;

	CURSORLOCK(this);

    if (frame != shmem->frame)
    {
        if (__private->cursorFlags[frame])
        {
            hwCursorActive = _setCursorImage( frame );
            __private->cursorFlags[frame] = hwCursorActive ? kIOFBCursorHWCapable : 0;
        }
        else
            hwCursorActive = 0;
        shmem->frame = frame;
        shmem->hardwareCursorActive = hwCursorActive;
    }

    shmem->cursorLoc = *cursorLoc;
    if (shmem->shieldFlag
            && ((0 == shmem->hardwareCursorActive) || (shmem->hardwareCursorShields)))
        CheckShield(this);

    SysShowCursor(this);

	CURSORUNLOCK(this);
}

void IOFramebuffer::updateVBL(OSObject * owner, IOTimerEventSource * sender)
{
    IOFB_START(updateVBL,0,0,0);
    IOFramebuffer * inst = (IOFramebuffer *) owner;

	if (inst->__private->vblInterrupt
			&& inst->__private->vblThrottle 
			&& inst->__private->displaysOnline
			&& inst->pagingState)
	{
		inst->setInterruptState(inst->__private->vblInterrupt, kEnabledInterruptState);
	}
    IOFB_END(updateVBL,0,0,0);
}

void IOFramebuffer::deferredVBLDisable(OSObject * owner,
                                       IOInterruptEventSource * evtSrc, int intCount)
{
    IOFB_START(deferredVBLDisable,intCount,0,0);
    IOFramebuffer * inst = (IOFramebuffer *) owner;

	if (inst->__private->vblInterrupt && inst->__private->vblThrottle)
	{
		inst->setInterruptState(inst->__private->vblInterrupt, kDisabledInterruptState);
	}
    IOFB_END(deferredVBLDisable,0,0,0);
}

bool IOFramebuffer::getTimeOfVBL(AbsoluteTime * deadlineAT, uint32_t frames)
{
    IOFB_START(getTimeOfVBL,frames,0,0);
	uint64_t last, now;
	uint64_t delta;

    StdFBShmem_t * shmem = GetShmem(this);
    if (!shmem)
    {
        IOFB_END(getTimeOfVBL,false,0,0);
        return (false);
    }

	do
	{
		last = AbsoluteTime_to_scalar(&shmem->vblTime);
		now = mach_absolute_time();
	}
	while (last != AbsoluteTime_to_scalar(&shmem->vblTime));

	delta = AbsoluteTime_to_scalar(&shmem->vblDeltaReal);
	if (delta)
	{
		now += frames * delta - ((now - last) % delta);
	}
	AbsoluteTime_to_scalar(deadlineAT) = now;

    IOFB_END(getTimeOfVBL,true,0,0);
	return (true);
}

void IOFramebuffer::handleVBL(IOFramebuffer * inst, void * ref)
{
    IOFB_START(handleVBL,0,0,0);
    StdFBShmem_t * shmem = GetShmem(inst);
    AbsoluteTime   now;
    uint64_t       _now, calculatedDelta, drift;

    if (!shmem)
    {
        IOFB_END(handleVBL,-1,__LINE__,0);
        return;
    }
	inst->__private->actualVBLCount++;

    _now = mach_absolute_time();
    AbsoluteTime_to_scalar(&now) = _now;
	AbsoluteTime delta = now;
	SUB_ABSOLUTETIME(&delta, &shmem->vblTime);
	calculatedDelta = AbsoluteTime_to_scalar(&shmem->vblDeltaReal);

	if (calculatedDelta && inst->__private->vblThrottle)
	{
		// round up to deal with scheduling noise.
		uint64_t timeCount = ((AbsoluteTime_to_scalar(&delta) * 2 / calculatedDelta) + 1) / 2;
		shmem->vblCount += timeCount;
		timeCount = (timeCount > inst->__private->actualVBLCount)
					? (timeCount - inst->__private->actualVBLCount)
					: (inst->__private->actualVBLCount - timeCount);
		if (gIOFBVBLDrift && (timeCount <= 1))
		{
			drift = (AbsoluteTime_to_scalar(&shmem->vblTime) 
					 + inst->__private->actualVBLCount * calculatedDelta);
			drift = (drift < _now) ? (_now - drift) : (drift - _now);
			shmem->vblDrift = drift;
			shmem->vblDeltaMeasured = (AbsoluteTime_to_scalar(&delta) / inst->__private->actualVBLCount);
//   		shmem->vblDeltaMeasured = IOMappedRead32(0xb0070000);
		}
	}
	else
	{
		shmem->vblCount++;
		shmem->vblDelta = delta;
		shmem->vblDeltaReal = delta;
	}
    shmem->vblTime  = now;
	inst->__private->actualVBLCount = 0;

    KERNEL_DEBUG(0xc000030 | DBG_FUNC_NONE,
                 (uint32_t)(AbsoluteTime_to_scalar(&shmem->vblDeltaReal) >> 32),
                 (uint32_t)(AbsoluteTime_to_scalar(&shmem->vblDeltaReal)), 0, 0, 0);

    if (inst->vblSemaphore)
        semaphore_signal_all(inst->vblSemaphore);

	if (inst->__private->vblThrottle)
	{
        if (!gIOFBVBLDrift) inst->__private->deferredVBLDisableEvent->interruptOccurred(0, 0, 0);
		inst->__private->vblUpdateTimer->setTimeoutMS(kVBLThrottleTimeMS);
	}
	else if (inst->__private->deferredCLUTSetEvent 
           && (inst->__private->gammaNeedSet || inst->__private->clutDataLen || inst->__private->waitVBLEvent))
	{
        inst->__private->deferredCLUTSetEvent->interruptOccurred(0, 0, 0);
	}

    IOFB_END(handleVBL,0,0,0);
}

void IOFramebuffer::resetCursor( void )
{
    IOFB_START(resetCursor,0,0,0);
    StdFBShmem_t *      shmem;
    int                 frame;

    if (isInactive()) {
        IOFB_END(resetCursor,-2,0,0);
        return; // rdar://30475917
    }

    shmem = GetShmem(this);
    //    hwCursorLoaded = false;
    if (!shmem)
    {
        IOFB_END(resetCursor,-1,0,0);
        return;
    }

    shmem->hardwareCursorActive = 0;
    frame = shmem->frame;
    shmem->frame = frame ^ 1;
    showCursor( &shmem->cursorLoc, frame );
    IOFB_END(resetCursor,0,0,0);
}

void IOFramebuffer::getVBLTime( AbsoluteTime * time, AbsoluteTime * delta )
{
    IOFB_START(getVBLTime,0,0,0);
    StdFBShmem_t *shmem;

    shmem = GetShmem(this);
    if (shmem && !isInactive()) // && !isInactive() rdar://30475917
    {
		getTimeOfVBL(time, 0);
        *delta = shmem->vblDeltaReal;
    }
    else
    {
        AbsoluteTime_to_scalar(time) = 0;
        AbsoluteTime_to_scalar(delta) = 0;
    }
    IOFB_END(getVBLTime,0,0,0);
}

void IOFramebuffer::getBoundingRect( IOGBounds ** bounds )
{
    IOFB_START(getBoundingRect,0,0,0);
    StdFBShmem_t *shmem;

    shmem = GetShmem(this);
    if ((NULL == shmem) || isInactive()) // || isInactive() rdar://30475917
        *bounds = NULL;
    else
        *bounds = &__private->screenBounds[0];
    IOFB_END(getBoundingRect,0,0,0);
}

//
// END:         Implementation of the evScreen protocol
//

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::getNotificationSemaphore(
                                                 IOSelect /*interruptType*/, semaphore_t * /*semaphore*/ )
{
    IOFB_START(getNotificationSemaphore,0,0,0);
    kern_return_t   kr = kIOReturnUnsupported;
    IOFB_END(getNotificationSemaphore,kr,0,0);
    return (kr);
}

IOReturn IOFramebuffer::extSetCursorVisible(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extSetCursorVisible,0,0,0);
    IOFramebuffer * inst    = (IOFramebuffer *) target;
    bool            visible = args->scalarInput[0];

    IOReturn            err;
    IOGPoint *          hs;
    StdFBShmem_t *      shmem;

    if ((err = inst->extEntry(true, kIOGReportAPIState_SetCursorVisible)))
    {
        IOFB_END(extSetCursorVisible,err,__LINE__,0);
        return (err);
    }

    shmem = GetShmem(inst);
    if (shmem->hardwareCursorActive && inst->__private->online)
    {
        hs = &shmem->hotSpot[0 != shmem->frame];
        err = inst->_setCursorState(
                  shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx,
                  shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny,
                  visible );

        if (inst->__private->cursorToDo)
            KICK_CURSOR(inst->__private->cursorThread);
    }
    else
        err = kIOReturnBadArgument;

    inst->extExit(err, kIOGReportAPIState_SetCursorVisible);

    IOFB_END(extSetCursorVisible,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::extSetCursorPosition(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extSetCursorPosition,0,0,0);
//    IOFramebuffer * inst = (IOFramebuffer *) target;
//    SInt32          x    = args->scalarInput[0];
//    SInt32          y    = args->scalarInput[1];

//    kIOGReportAPIState_SetCursorPosition

    IOFB_END(extSetCursorPosition,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

void IOFramebuffer::transformCursor( StdFBShmem_t * shmem, IOIndex frame )
{
    IOFB_START(transformCursor,frame,0,0);
    void *                    buf;
    unsigned int * out;
    volatile unsigned int   * cursPtr32 = 0;
    volatile unsigned short * cursPtr16 = 0;
    // <rdar://problem/31392057> IOFramebuffer::transformCursor: signed comparisons from lengths in shared memory lead to uninitialized heap reads and buffer overflow
    // Replace SInt32 with UInt32 and add appropriate casting.
    UInt32 x, y, dw, dh, sx, sy, sw, sh;

    if (__private->cursorBytesPerPixel == 4)
    {
        cursPtr32 = (volatile unsigned int *) __private->cursorImages[frame];
        cursPtr16 = 0;
    }
    else if (__private->cursorBytesPerPixel == 2)
    {
        cursPtr32 = 0;
        cursPtr16 = (volatile unsigned short *) __private->cursorImages[frame];
    }

    sw = static_cast<UInt32>(shmem->cursorSize[0 != frame].width);
    sh = static_cast<UInt32>(shmem->cursorSize[0 != frame].height);

    // <rdar://problem/22099296> potential integer overflow in IOFramebuffer::transformCursor
    if (sw > static_cast<UInt32>(maxCursorSize.width))
        sw = static_cast<UInt32>(maxCursorSize.width);
    if (sh > static_cast<UInt32>(maxCursorSize.height))
        sh = static_cast<UInt32>(maxCursorSize.height);

    if (kIOFBSwapAxes & __private->transform)
    {
        dw = sh;
        dh = sw;
    }
    else
    {
        dw = sw;
        dh = sh;
    }

    if ((0 == sw) && (0 == sh))
    {
        IOFB_END(transformCursor,-1,0,0);
        return;
    }

    buf = IOMalloc(dw * dh * __private->cursorBytesPerPixel);
    if (NULL != buf)
    {
        out = (unsigned int *) buf;

        for (y = 0; y < dh; y++)
        {
            for (x = 0; x < dw; x++)
            {
                if (kIOFBSwapAxes & __private->transform)
                {
                    sx = y;
                    sy = x;
                }
                else
                {
                    sx = x;
                    sy = y;
                }
                if (__private->transform & kIOFBInvertY)
                    sx = sw - sx - 1;
                if (__private->transform & kIOFBInvertX)
                    sy = sh - sy - 1;

                if (cursPtr32)
                    *out++ = cursPtr32[sx + sy * sw];
                else
                    STOREINC(out, cursPtr16[sx + sy * sw], UInt16)
                    }
        }

        bcopy(buf, (void *) __private->cursorImages[frame], dw * dh * __private->cursorBytesPerPixel);
        IOFree(buf, dw * dh * __private->cursorBytesPerPixel);

        shmem->cursorSize[0 != frame].width  = static_cast<SInt16>(dw);
        shmem->cursorSize[0 != frame].height = static_cast<SInt16>(dh);

        if (kIOFBSwapAxes & __private->transform)
        {
            x = static_cast<UInt32>(shmem->hotSpot[0 != frame].y);
            y = static_cast<UInt32>(shmem->hotSpot[0 != frame].x);
        }
        else
        {
            x = static_cast<UInt32>(shmem->hotSpot[0 != frame].x);
            y = static_cast<UInt32>(shmem->hotSpot[0 != frame].y);
        }
        if (__private->transform & kIOFBInvertX)
            x = dw - x - 1;
        if (__private->transform & kIOFBInvertY)
            y = dh - y - 1;
        shmem->hotSpot[0 != frame].x = static_cast<SInt16>(x);
        shmem->hotSpot[0 != frame].y = static_cast<SInt16>(y);
    }

    IOFB_END(transformCursor,0,0,0);
}

IOReturn IOFramebuffer::extSetNewCursor(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extSetNewCursor,0,0,0);
    IOFramebuffer * inst    = (IOFramebuffer *) target;
    IOIndex         cursor  = static_cast<IOIndex>(args->scalarInput[0]);
    IOIndex         frame   = static_cast<IOIndex>(args->scalarInput[1]);
    IOOptionBits    options = static_cast<IOOptionBits>(args->scalarInput[2]);

    StdFBShmem_t *      shmem;
    IOReturn            err;
    UInt32              hwCursorActive;
        
    if ((err = inst->extEntry(false, kIOGReportAPIState_SetNewCursor)))
    {
        IOFB_END(extSetNewCursor,err,__LINE__,0);
        return (err);
    }

    shmem = GetShmem(inst);
    // assumes called with cursorSema held
    if (cursor || options || (((UInt32) frame) >= inst->__private->numCursorFrames))
        err = kIOReturnBadArgument;
    else
    {
        if (kIOFBRotateFlags & inst->__private->transform)
            inst->transformCursor(shmem, frame);

        if ((shmem->cursorSize[0 != frame].width > inst->maxCursorSize.width)
                || (shmem->cursorSize[0 != frame].height > inst->maxCursorSize.height))
            err = kIOReturnBadArgument;

        else if (inst->haveHWCursor)
        {
            if (frame == shmem->frame)
            {
                hwCursorActive = inst->_setCursorImage( frame );
                shmem->hardwareCursorActive = hwCursorActive;
                inst->__private->cursorFlags[frame] = hwCursorActive ? kIOFBCursorHWCapable : 0;
            }
            else
            {
                inst->__private->cursorFlags[frame] = kIOFBCursorImageNew;
            }
            err = kIOReturnSuccess;             // I guess
        }
        else
            err = kIOReturnUnsupported;
    }
    if (inst->__private->cursorToDo)
        KICK_CURSOR(inst->__private->cursorThread);

    inst->extExit(err, kIOGReportAPIState_SetNewCursor);

    IOFB_END(extSetNewCursor,err,0,0);
    return (err);
}

bool IOFramebuffer::convertCursorImage( void * cursorImage,
                                        IOHardwareCursorDescriptor * hwDesc,
                                        IOHardwareCursorInfo * hwCursorInfo )
{
    IOFB_START(convertCursorImage,0,0,0);
    StdFBShmem_t *              shmem = GetShmem(this);
    UInt8 *                     dataOut = hwCursorInfo->hardwareCursorData;
    IOColorEntry *              clut = hwCursorInfo->colorMap;
    UInt32                      maxColors = hwDesc->numColors;
    int                         frame = static_cast<int>(reinterpret_cast<uintptr_t>(cursorImage));

    volatile unsigned short *   cursPtr16;
    volatile unsigned int *     cursPtr32;
    SInt32                      x, lastx, y, lasty;
    UInt32                      width, height, lineBytes = 0;
    UInt32                      index, numColors = 0;
    UInt32                      alpha, red, green, blue;
    UInt16                      s16;
    UInt32                      s32;
    UInt32                      pixel = 0;
    UInt32                      data = 0;
    UInt32                      bits = 0;
    bool                        ok = true;
    bool                        isDirect;

    if (__private->testingCursor)
    {
        IOHardwareCursorDescriptor copy;

		copy = *hwDesc;
		copy.colorEncodings = NULL;
        if ((hwDesc->numColors == 0) && (hwDesc->bitDepth > 8) && (hwDesc->bitDepth < 32))
        {
            copy.bitDepth = 32;
        }
        OSData * tdata = OSData::withBytes( &copy, sizeof(IOHardwareCursorDescriptor) );
        if (tdata)
        {
            __private->cursorAttributes->setObject( tdata );
            tdata->release();
        }

        IOFB_END(convertCursorImage,false,__LINE__,0);
        return (false);
    }
    else if (!hwCursorInfo || !hwCursorInfo->hardwareCursorData)
    {
        IOFB_END(convertCursorImage,false,__LINE__,0);
        return (false);
    }

    if (static_cast<UInt32>(frame) >= __private->numCursorFrames)
    {
        IOFB_END(convertCursorImage,false,__LINE__,0);
        return (false);
    }

    if (__private->cursorBytesPerPixel == 4)
    {
        cursPtr32 = (volatile unsigned int *) __private->cursorImages[frame];
        cursPtr16 = 0;
    }
    else if (__private->cursorBytesPerPixel == 2)
    {
        cursPtr32 = 0;
        cursPtr16 = (volatile unsigned short *) __private->cursorImages[frame];
    }
    else
    {
        IOFB_END(convertCursorImage,false,__LINE__,0);
        return (false);
    }

	if (!cursPtr32 && !cursPtr16)
    {
        IOFB_END(convertCursorImage,false,__LINE__,0);
        return (false);
    }

    x = shmem->cursorSize[0 != frame].width;
    y = shmem->cursorSize[0 != frame].height;
    if ((x > (SInt32) hwDesc->width) || (y > (SInt32) hwDesc->height))
    {
        IOFB_END(convertCursorImage,false,__LINE__,0);
        return (false);
    }
    isDirect = (hwDesc->bitDepth > 8);
    if (isDirect && (hwDesc->bitDepth != 32) && (hwDesc->bitDepth != 16))
    {
        IOFB_END(convertCursorImage,false,__LINE__,0);
        return (false);
    }

    width  = hwDesc->width;
    height = hwDesc->height;

    // matrox workaround - 2979661
    if ((maxColors > 1) && (&clut[1] == (IOColorEntry *) hwCursorInfo))
        width = height = 16;
    // --

    SInt32 adjX = 4 - shmem->hotSpot[0 != frame].x;
    SInt32 adjY = 4 - shmem->hotSpot[0 != frame].y;
    if ((adjX < 0) || ((UInt32)(x + adjX) > width))
        adjX = 0;
    else
        x += adjX;
    if ((adjY < 0) || ((UInt32)(y + adjY) > height))
        adjY = 0;

    __private->cursorHotSpotAdjust[0 != frame].x = adjX;
    __private->cursorHotSpotAdjust[0 != frame].y = adjY;

    while ((width >> 1) >= (UInt32) x)
        width >>= 1;
    while ((UInt32)(height >> 1) >= (UInt32)(y + adjY))
        height >>= 1;

    hwCursorInfo->cursorWidth  = width;
    hwCursorInfo->cursorHeight = height;
    hwCursorInfo->cursorHotSpotX = shmem->hotSpot[0 != frame].x 
                                 + __private->cursorHotSpotAdjust[0 != frame].x;
    hwCursorInfo->cursorHotSpotY = shmem->hotSpot[0 != frame].y 
                                 + __private->cursorHotSpotAdjust[0 != frame].y;
    lastx = x - width - 1;

    if (isDirect && adjY)
    {
        lineBytes = width * (hwDesc->bitDepth >> 3);
        // top lines
        bzero_nc( dataOut, adjY * lineBytes );
        dataOut += adjY * lineBytes;
        // bottom lines
        adjY    = height - shmem->cursorSize[0 != frame].height - adjY;
        lasty   = -1;
    }
    else
    {
        y += adjY;
        lasty = y - height - 1;
    }

    while (ok && (--y != lasty))
    {
        x = shmem->cursorSize[0 != frame].width + adjX;
        while (ok && (--x != lastx))
        {
            if ((x < 0)
                    || (y < 0)
                    || (x >= shmem->cursorSize[0 != frame].width)
                    || (y >= shmem->cursorSize[0 != frame].height))
                alpha = red = green = blue = 0;

            else if (cursPtr32)
            {
                s32 = *(cursPtr32++);
                alpha = (s32 >> 24) & 0xff;
                red = (s32 >> 16) & 0xff;
                green = (s32 >> 8) & 0xff;
                blue = (s32) & 0xff;
            }
            else
            {
#define RMASK16 0xF000
#define GMASK16 0x0F00
#define BMASK16 0x00F0
#define AMASK16 0x000F
                s16 = *(cursPtr16++);
                alpha = s16 & AMASK16;
                alpha |= (alpha << 4);
                red = (s16 & RMASK16) >> 8;
                red |= (red >> 4);
                green = (s16 & GMASK16) >> 4;
                green |= (green >> 4);
                blue = s16 & BMASK16;
                blue |= (blue >> 4);
            }

            if (isDirect)
            {
                if (alpha == 0)
                {
                    if (0xff == (red & green & blue))
                    {
                        /* Transparent white area.  Invert dst. */
                        if (kInvertingEncodedPixel
                                & hwDesc->supportedSpecialEncodings)
                            pixel = hwDesc->specialEncodings[kInvertingEncoding];
                        else
                            ok = false;
                    }
                    else
                        pixel = 0;

                    if (hwDesc->bitDepth == 32)
                        STOREINC(dataOut, pixel, UInt32)
                    else
                        STOREINC(dataOut, pixel, UInt16)
                }
                else
                {
                    if (0xff != alpha)
                    {
                        red   = 0xff * red   / alpha;
                        green = 0xff * green / alpha;
                        blue  = 0xff * blue  / alpha;
                    }
                    if (hwDesc->bitDepth == 32)
                    {
                        pixel =   (alpha << 24)
                                  | ((red   & 0xff) << 16)
                                  | ((green & 0xff) << 8)
                                  | (blue   & 0xff);

                        STOREINC(dataOut, pixel, UInt32)
                    }
                    else
                    {
                        pixel =   ((alpha & 0xf0) << 8)
                                  | ((red   & 0xf0) << 4)
                                  | ((green & 0xf0) << 0)
                                  | ((blue  & 0xf0) >> 4);

                        STOREINC(dataOut, pixel, UInt16)
                    }
                }
            }
            else
            {
                /* Indexed pixels */

                if (alpha == 0)
                {
                    if (0 == (red | green | blue))
                    {
                        /* Transparent black area.  Leave dst as is. */
                        if (kTransparentEncodedPixel
                                & hwDesc->supportedSpecialEncodings)
                            pixel = hwDesc->specialEncodings[kTransparentEncoding];
                        else
                            ok = false;
                    }
                    else if (0xff == (red & green & blue))
                    {
                        /* Transparent white area.  Invert dst. */
                        if (kInvertingEncodedPixel
                                & hwDesc->supportedSpecialEncodings)
                            pixel = hwDesc->specialEncodings[kInvertingEncoding];
                        else
                            ok = false;
                    }
                    else
                        ok = false;
                }
                else if (alpha == 0xff)
                {
                    red   |= (red << 8);
                    green |= (green << 8);
                    blue  |= (blue << 8);

                    /* Opaque cursor pixel.  Mark it. */
                    for (index = 0; index < numColors; index++)
                    {
                        if ((red   == clut[index].red)
                                && (green == clut[index].green)
                                && (blue  == clut[index].blue))
                        {
                            pixel = clut[index].index;
                            break;
                        }
                    }
                    if (index == numColors)
                    {
                        ok = (numColors < maxColors);
                        if (ok)
                        {
                            pixel = hwDesc->colorEncodings[numColors++];
                            clut[index].red   = red;
                            clut[index].green = green;
                            clut[index].blue  = blue;
                            clut[index].index = pixel;
                        }
                    }
                }
                else
                {
                    /* Alpha is not 0 or 1.0.  Sover the cursor. */
                    ok = false;
                    break;
                }
                data <<= hwDesc->bitDepth;
                data |= pixel;
                bits += hwDesc->bitDepth;
                if (0 == (bits & 31))
                {
                    OSWriteBigInt32(dataOut, 0, data);
                    dataOut += sizeof(UInt32);
                }
            }
        } /* x */
    } /* y */

    if (ok && isDirect && adjY)
    {
        // bottom lines
        bzero_nc( dataOut, adjY * lineBytes );
        dataOut += adjY * lineBytes;
    }

    __private->cursorClutDependent = (ok && !isDirect);

#if 0
    if (ok)
    {
        static UInt32 lastWidth;
        static UInt32 lastHeight;

        if ((width != lastWidth) || (height != lastHeight))
        {
            lastWidth = width;
            lastHeight = height;
            IOLog("[%d,%d]", width, height);
        }

        if (((UInt32)(dataOut - hwCursorInfo->hardwareCursorData)
                != ((hwCursorInfo->cursorHeight * hwCursorInfo->cursorWidth * hwDesc->bitDepth) >> 3)))
            IOLog("dataOut %p, %p @ %d\n", dataOut, hwCursorInfo->hardwareCursorData, hwDesc->bitDepth );
    }
#endif

    IOFB_END(convertCursorImage,ok,0,0);
    return (ok);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Apple standard 8-bit CLUT

#if 0
UInt8 appleClut8[256 * 3] =
{
    // 00
    0xFF,0xFF,0xFF, 0xFF,0xFF,0xCC,     0xFF,0xFF,0x99, 0xFF,0xFF,0x66,
    0xFF,0xFF,0x33, 0xFF,0xFF,0x00,     0xFF,0xCC,0xFF, 0xFF,0xCC,0xCC,
    0xFF,0xCC,0x99, 0xFF,0xCC,0x66,     0xFF,0xCC,0x33, 0xFF,0xCC,0x00,
    0xFF,0x99,0xFF, 0xFF,0x99,0xCC,     0xFF,0x99,0x99, 0xFF,0x99,0x66,
    // 10
    0xFF,0x99,0x33, 0xFF,0x99,0x00,     0xFF,0x66,0xFF, 0xFF,0x66,0xCC,
    0xFF,0x66,0x99, 0xFF,0x66,0x66,     0xFF,0x66,0x33, 0xFF,0x66,0x00,
    0xFF,0x33,0xFF, 0xFF,0x33,0xCC,     0xFF,0x33,0x99, 0xFF,0x33,0x66,
    0xFF,0x33,0x33, 0xFF,0x33,0x00,     0xFF,0x00,0xFF, 0xFF,0x00,0xCC,
    // 20
    0xFF,0x00,0x99, 0xFF,0x00,0x66,     0xFF,0x00,0x33, 0xFF,0x00,0x00,
    0xCC,0xFF,0xFF, 0xCC,0xFF,0xCC,     0xCC,0xFF,0x99, 0xCC,0xFF,0x66,
    0xCC,0xFF,0x33, 0xCC,0xFF,0x00,     0xCC,0xCC,0xFF, 0xCC,0xCC,0xCC,
    0xCC,0xCC,0x99, 0xCC,0xCC,0x66,     0xCC,0xCC,0x33, 0xCC,0xCC,0x00,
    // 30
    0xCC,0x99,0xFF, 0xCC,0x99,0xCC,     0xCC,0x99,0x99, 0xCC,0x99,0x66,
    0xCC,0x99,0x33, 0xCC,0x99,0x00,     0xCC,0x66,0xFF, 0xCC,0x66,0xCC,
    0xCC,0x66,0x99, 0xCC,0x66,0x66,     0xCC,0x66,0x33, 0xCC,0x66,0x00,
    0xCC,0x33,0xFF, 0xCC,0x33,0xCC,     0xCC,0x33,0x99, 0xCC,0x33,0x66,
    // 40
    0xCC,0x33,0x33, 0xCC,0x33,0x00,     0xCC,0x00,0xFF, 0xCC,0x00,0xCC,
    0xCC,0x00,0x99, 0xCC,0x00,0x66,     0xCC,0x00,0x33, 0xCC,0x00,0x00,
    0x99,0xFF,0xFF, 0x99,0xFF,0xCC,     0x99,0xFF,0x99, 0x99,0xFF,0x66,
    0x99,0xFF,0x33, 0x99,0xFF,0x00,     0x99,0xCC,0xFF, 0x99,0xCC,0xCC,
    // 50
    0x99,0xCC,0x99, 0x99,0xCC,0x66,     0x99,0xCC,0x33, 0x99,0xCC,0x00,
    0x99,0x99,0xFF, 0x99,0x99,0xCC,     0x99,0x99,0x99, 0x99,0x99,0x66,
    0x99,0x99,0x33, 0x99,0x99,0x00,     0x99,0x66,0xFF, 0x99,0x66,0xCC,
    0x99,0x66,0x99, 0x99,0x66,0x66,     0x99,0x66,0x33, 0x99,0x66,0x00,
    // 60
    0x99,0x33,0xFF, 0x99,0x33,0xCC,     0x99,0x33,0x99, 0x99,0x33,0x66,
    0x99,0x33,0x33, 0x99,0x33,0x00,     0x99,0x00,0xFF, 0x99,0x00,0xCC,
    0x99,0x00,0x99, 0x99,0x00,0x66,     0x99,0x00,0x33, 0x99,0x00,0x00,
    0x66,0xFF,0xFF, 0x66,0xFF,0xCC,     0x66,0xFF,0x99, 0x66,0xFF,0x66,
    // 70
    0x66,0xFF,0x33, 0x66,0xFF,0x00,     0x66,0xCC,0xFF, 0x66,0xCC,0xCC,
    0x66,0xCC,0x99, 0x66,0xCC,0x66,     0x66,0xCC,0x33, 0x66,0xCC,0x00,
    0x66,0x99,0xFF, 0x66,0x99,0xCC,     0x66,0x99,0x99, 0x66,0x99,0x66,
    0x66,0x99,0x33, 0x66,0x99,0x00,     0x66,0x66,0xFF, 0x66,0x66,0xCC,
    // 80
    0x66,0x66,0x99, 0x66,0x66,0x66,     0x66,0x66,0x33, 0x66,0x66,0x00,
    0x66,0x33,0xFF, 0x66,0x33,0xCC,     0x66,0x33,0x99, 0x66,0x33,0x66,
    0x66,0x33,0x33, 0x66,0x33,0x00,     0x66,0x00,0xFF, 0x66,0x00,0xCC,
    0x66,0x00,0x99, 0x66,0x00,0x66,     0x66,0x00,0x33, 0x66,0x00,0x00,
    // 90
    0x33,0xFF,0xFF, 0x33,0xFF,0xCC,     0x33,0xFF,0x99, 0x33,0xFF,0x66,
    0x33,0xFF,0x33, 0x33,0xFF,0x00,     0x33,0xCC,0xFF, 0x33,0xCC,0xCC,
    0x33,0xCC,0x99, 0x33,0xCC,0x66,     0x33,0xCC,0x33, 0x33,0xCC,0x00,
    0x33,0x99,0xFF, 0x33,0x99,0xCC,     0x33,0x99,0x99, 0x33,0x99,0x66,
    // a0
    0x33,0x99,0x33, 0x33,0x99,0x00,     0x33,0x66,0xFF, 0x33,0x66,0xCC,
    0x33,0x66,0x99, 0x33,0x66,0x66,     0x33,0x66,0x33, 0x33,0x66,0x00,
    0x33,0x33,0xFF, 0x33,0x33,0xCC,     0x33,0x33,0x99, 0x33,0x33,0x66,
    0x33,0x33,0x33, 0x33,0x33,0x00,     0x33,0x00,0xFF, 0x33,0x00,0xCC,
    // b0
    0x33,0x00,0x99, 0x33,0x00,0x66,     0x33,0x00,0x33, 0x33,0x00,0x00,
    0x00,0xFF,0xFF, 0x00,0xFF,0xCC,     0x00,0xFF,0x99, 0x00,0xFF,0x66,
    0x00,0xFF,0x33, 0x00,0xFF,0x00,     0x00,0xCC,0xFF, 0x00,0xCC,0xCC,
    0x00,0xCC,0x99, 0x00,0xCC,0x66,     0x00,0xCC,0x33, 0x00,0xCC,0x00,
    // c0
    0x00,0x99,0xFF, 0x00,0x99,0xCC,     0x00,0x99,0x99, 0x00,0x99,0x66,
    0x00,0x99,0x33, 0x00,0x99,0x00,     0x00,0x66,0xFF, 0x00,0x66,0xCC,
    0x00,0x66,0x99, 0x00,0x66,0x66,     0x00,0x66,0x33, 0x00,0x66,0x00,
    0x00,0x33,0xFF, 0x00,0x33,0xCC,     0x00,0x33,0x99, 0x00,0x33,0x66,
    // d0
    0x00,0x33,0x33, 0x00,0x33,0x00,     0x00,0x00,0xFF, 0x00,0x00,0xCC,
    0x00,0x00,0x99, 0x00,0x00,0x66,     0x00,0x00,0x33, 0xEE,0x00,0x00,
    0xDD,0x00,0x00, 0xBB,0x00,0x00,     0xAA,0x00,0x00, 0x88,0x00,0x00,
    0x77,0x00,0x00, 0x55,0x00,0x00,     0x44,0x00,0x00, 0x22,0x00,0x00,
    // e0
    0x11,0x00,0x00, 0x00,0xEE,0x00,     0x00,0xDD,0x00, 0x00,0xBB,0x00,
    0x00,0xAA,0x00, 0x00,0x88,0x00,     0x00,0x77,0x00, 0x00,0x55,0x00,
    0x00,0x44,0x00, 0x00,0x22,0x00,     0x00,0x11,0x00, 0x00,0x00,0xEE,
    0x00,0x00,0xDD, 0x00,0x00,0xBB,     0x00,0x00,0xAA, 0x00,0x00,0x88,
    // f0
    0x00,0x00,0x77, 0x00,0x00,0x55,     0x00,0x00,0x44, 0x00,0x00,0x22,
    0x00,0x00,0x11, 0xEE,0xEE,0xEE,     0xDD,0xDD,0xDD, 0xBB,0xBB,0xBB,
    0xAA,0xAA,0xAA, 0x88,0x88,0x88,     0x77,0x77,0x77, 0x55,0x55,0x55,
    0x44,0x44,0x44, 0x22,0x22,0x22,     0x11,0x11,0x11, 0x00,0x00,0x00
};
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int killprint;
extern "C"
{
    int kmputc( int c );
}

#if DOANIO

#warning ** DO AN IO **

static unsigned long doaniobuf[256];

static void doanio( void )
{
    struct uio uio;
    struct iovec iovec;
    int err;
    dev_t device = makedev( 14, 0 );

    iovec.iov_base = (char *) &doaniobuf[0];
    iovec.iov_len  = 1024;

    uio.uio_iov = &iovec;
    uio.uio_iovcnt = 1;
    uio.uio_rw = UIO_READ;
    uio.uio_segflg = UIO_SYSSPACE;
    uio.uio_offset = 0;
    uio.uio_resid = 1024;

    DEBG("", "\n");
    err = ((*cdevsw[major(device)].d_read)(device, &uio, 0));
    DEBG("", " done(%08lx)\n", doaniobuf[0]);
}

#endif

IOReturn IOFramebuffer::deliverDisplayModeDidChangeNotification( void )
{
    IOFB_START(deliverDisplayModeDidChangeNotification,0,0,0);
    IOReturn ret = deliverFramebufferNotification( kIOFBNotifyDisplayModeDidChange );

    if (__private->lastNotifyOnline != __private->online)
    {
        __private->lastNotifyOnline = __private->online;
        deliverFramebufferNotification( kIOFBNotifyOnlineChange, (void *)(uintptr_t) __private->online);
    }

    IOFB_END(deliverDisplayModeDidChangeNotification,ret,0,0);
    return (ret);
}

void IOFramebuffer::saveFramebuffer(void)
{
    IOFB_START(saveFramebuffer,0,0,0);
#if VRAM_SAVE
	// vram content is being lost
	uintptr_t           value;
    FB_START(getAttribute,kIOVRAMSaveAttribute,__LINE__,0);
    IOReturn err = getAttribute(kIOVRAMSaveAttribute, &value);
    FB_END(getAttribute,err,__LINE__,0);

	if (true
			&& !dead
			&& !__private->saveLength
			&& __private->online
			&& __private->framebufferHeight
			&& __private->framebufferWidth
			&& rowBytes
			&& (kIOReturnSuccess == err)
			&& value)
	{
		vm_size_t sLen;
		sLen = __private->framebufferHeight * rowBytes;

		/*
		* dLen should account for possible growth. (e.g. run-length encoding noise)
		*       Add 5 bytes for header,
		*       12% for RLE growth
		*       2 bytes per line for line spans,
		*       1 additional escape code byte for trailing pixel in each line
		*/
#if VRAM_COMPRESS
		vm_size_t dLen;
		uint32_t  idx;

		dLen = 5 + sLen + ((sLen + 7) >> 3) + (__private->framebufferHeight * 3) + rowBytes;

		for (idx = 0;
			 (idx < kIOPreviewImageCount) && __private->saveBitsMD[idx];
			 idx++)	{}

		dLen *= (idx + 1);
		DEBG1(thisName, " dLen 0x%x\n", (int) dLen);

		dLen = round_page(dLen);
		if (dLen >= 96*1024*1024) dLen = 95*1024*1024;
		__private->saveLength = static_cast<uint32_t>(dLen);
#else
		__private->saveLength = round_page(sLen);
#endif
		__private->saveFramebuffer = IOMallocPageable( __private->saveLength, page_size );
		if (!__private->saveFramebuffer)
			__private->saveLength = 0;
		else
		{
#if VRAM_COMPRESS
			UInt8 *       bits[kIOPreviewImageCount] = { 0 };
			IOByteCount   bitsLen = 0;
			IOMemoryMap * map[kIOPreviewImageCount] = { 0 };

			for (idx = 0;
				 (idx < kIOPreviewImageCount) && __private->saveBitsMD[idx];
				 idx++)
			{
				map[idx] = __private->saveBitsMD[idx]->map(kIOMapReadOnly);
				if (map[idx])
				{
					bits[idx] = (UInt8 *) map[idx]->getVirtualAddress();
					if (!idx) bitsLen = map[idx]->getLength();
					else if (bitsLen != map[idx]->getLength())
					{
						bits[idx] = NULL;
						map[idx]->release();
						map[idx] = 0;
					}
				}
				__private->saveBitsMD[idx]->release();
				__private->saveBitsMD[idx] = 0;
			}

			if (!bits[0])
			{
				IOLog("%s: no save bits\n", thisName);
				dLen = 0;
			}
			else if ((((__private->framebufferHeight - 1) * rowBytes)
				 + __private->framebufferWidth * bytesPerPixel) > bitsLen)
			{
				IOLog("%s: bad pixel parameters %d x %d x %d > 0x%x\n", thisName,
				 (int) __private->framebufferWidth, (int) __private->framebufferHeight, (int) rowBytes, (int) bitsLen);
				dLen = 0;
			}
			else
			{
                uint32_t pixelBits;
                if (__private->pixelInfo.bitsPerComponent > 8)
                    pixelBits = __private->pixelInfo.componentCount * __private->pixelInfo.bitsPerComponent;
                else
                    pixelBits = __private->pixelInfo.bitsPerPixel;

                uint8_t * saveGammaData;
                uint32_t saveGammaChannelCount;
                uint32_t saveGammaDataCount;
                uint32_t saveGammaDataWidth;
                // Preference to hibernateGamma
                if (NULL != __private->hibernateGammaData)
                {
                    saveGammaData = __private->hibernateGammaData;
                    saveGammaChannelCount = __private->hibernateGammaChannelCount;
                    saveGammaDataCount = __private->hibernateGammaDataCount;
                    saveGammaDataWidth = __private->hibernateGammaDataWidth;
                }
                else
                {
                    saveGammaData = __private->rawGammaData;
                    saveGammaChannelCount = __private->rawGammaChannelCount;
                    saveGammaDataCount = __private->rawGammaDataCount;
                    saveGammaDataWidth = __private->rawGammaDataWidth;
                }

                dLen = CompressData( bits, kIOPreviewImageCount, bytesPerPixel, pixelBits,
                                    __private->framebufferWidth, __private->framebufferHeight, rowBytes,
                                    (UInt8 *) __private->saveFramebuffer, __private->saveLength,
                                    saveGammaChannelCount, saveGammaDataCount,
                                    saveGammaDataWidth, saveGammaData);
			}
			DEBG1(thisName, " compressed to %d%%\n", (int) ((dLen * 100) / sLen));

			for (idx = 0; (idx < kIOPreviewImageCount); idx++) 
			{
				if (map[idx]) map[idx]->release();
			}

			dLen = round_page( dLen );
			if (__private->saveLength > dLen)
			{
				IOFreePageable( (void *) (((uintptr_t) __private->saveFramebuffer) + dLen),
								__private->saveLength - dLen );
				__private->saveLength = static_cast<uint32_t>(dLen);
			}
#else
			if (fFrameBuffer) bcopy_nc( (void *) fFrameBuffer, __private->saveFramebuffer, sLen );
#endif
			if (__private->saveLength)
			{
#if RLOG
				kern_return_t kr = 
#endif
				vm_map_wire( IOPageableMapForAddress( (vm_address_t) __private->saveFramebuffer ),
							 (vm_address_t) __private->saveFramebuffer,
							 ((vm_address_t) __private->saveFramebuffer) + __private->saveLength,
							 VM_PROT_READ | VM_PROT_WRITE, FALSE );
				DEBG(thisName, " vm_map_wire(%x)\n", kr);

                if ((this == gIOFBConsoleFramebuffer) || (true == gIOFBSetPreviewImage))
				{
					IOMemoryDescriptor *
					previewBuffer = IOMemoryDescriptor::withAddress(
										__private->saveFramebuffer, 
										__private->saveLength, 
										kIODirectionInOut);
					if (previewBuffer)
					{
						getPMRootDomain()->setProperty(kIOHibernatePreviewBufferKey, previewBuffer);
						previewBuffer->release();

// This code is never executed
//						OSNumber * num;
//						if (false 
//						 && fFrameBuffer
//						 && (num = OSDynamicCast(OSNumber, getPMRootDomain()->getProperty(kIOHibernateModeKey))) 
//						 && (kIOHibernateModeOn == ((kIOHibernateModeSleep | kIOHibernateModeOn) & num->unsigned32BitValue()))
//						 && !gIOFBCurrentClamshellState)
//						{
//							// hibernate enabled, will power off, clamshell open - do preview
//							UInt32 flags = 0;
//							getProvider()->setProperty(kIOHibernatePreviewActiveKey, &flags, sizeof(flags));
//							PreviewDecompressData(&__private->saveFramebuffer, 0, (void *)fFrameBuffer,
//													__private->framebufferWidth, __private->framebufferHeight,
//													bytesPerPixel, rowBytes);
//						}
					}
				}
			}
		}
	}
#endif /* VRAM_SAVE */
    IOFB_END(saveFramebuffer,0,0,0);
}

IOReturn IOFramebuffer::restoreFramebuffer(IOIndex event)
{
    IOFB_START(restoreFramebuffer,event,0,0);
	IOReturn ret = kIOReturnNotReady;
	uint32_t restoreType;
    bool bPerformVRAMWrite = true;

	if (2 == pendingPowerState)
	{
		restoreType = 
			(kOSBooleanFalse == getPMRootDomain()->getProperty(kIOPMUserTriggeredFullWakeKey))
			? kIOScreenRestoreStateDark : kIOScreenRestoreStateNormal;
	}
	else restoreType = kIOScreenRestoreStateDark;

	if (__private->hibernateGfxStatus) restoreType = kIOScreenRestoreStateDark;

#if VRAM_SAVE
	// restore vram content
	if (restoreType != __private->restoreType)
	{
		thread_t saveThread = NULL;

		if (kIOFBNotifyVRAMReady == event)
		{
			IOMemoryDescriptor * vram;
            FB_START(getVRAMRange,0,__LINE__,0);
            vram = getVRAMRange();
            FB_END(getVRAMRange,0,__LINE__,0);
			if (vram)
			{
				vram->redirect( kernel_task, false );
				vram->release();
			}
		}
		else
		{
            saveThread = __private->controller->setPowerThread();
            FB_START(setAttribute,kIOFBSpeedAttribute,__LINE__,kIOFBVRAMCompressSpeed);
			setAttribute(kIOFBSpeedAttribute, kIOFBVRAMCompressSpeed);
            FB_END(setAttribute,0,__LINE__,0);
		}

		if (!__private->saveLength) restoreType = kIOScreenRestoreStateDark;

        // Fix for <rdar://problem/20429613> SEED: BUG: kernel panic after connecting to a display in closed clamshell@doSetPowerState
        if (fFrameBuffer)
        {
            // No fVramMap?!?  Don't attempt to fill the screen.
            if (fVramMap && kIOWSAA_Hibernate != __private->wsaaState)
            {
                // If the map length is below the current resolution, handle the error
                IOByteCount frameBufferLength = fVramMap->getLength();
                if (frameBufferLength < (rowBytes * __private->framebufferHeight))
                {
                    bPerformVRAMWrite = false;
#if 0 /*DEBUG*/
                    // Perform an extra check for the base.  Did it change?
                    IOPhysicalAddress64 base = fVramMap->getVirtualAddress();
                    if (base == (IOPhysicalAddress64)fFrameBuffer)
                    {
                        IOLog("%d: Invalid aperture range found for: %#llx (%u:%llu)\n", __LINE__, __private->regID, rowBytes * __private->framebufferHeight, frameBufferLength);
                        kprintf("%d: Invalid aperture range found for: %#llx (%u:%llu)\n", __LINE__, __private->regID, rowBytes * __private->framebufferHeight, frameBufferLength);
                    }
                    else
                    {
                        IOLog("%d: Invalid aperture base and range found for: %#llx (%#llx:%#llx, %u:%llu)\n", __LINE__, __private->regID, base, (IOPhysicalAddress64)fFrameBuffer, rowBytes * __private->framebufferHeight, frameBufferLength);
                        kprintf("%d: Invalid aperture base and range found for: %#llx (%#llx:%#llx, %u:%llu)\n", __LINE__, __private->regID, base, (IOPhysicalAddress64)fFrameBuffer, rowBytes * __private->framebufferHeight, frameBufferLength);
                    }
#endif /*DEBUG*/

                    // Attempt to remap and re-acquire the memory map, if this fails there is nothing we can do with the CPU
                    IOMemoryDescriptor * fbRange = getApertureRangeWithLength(kIOFBSystemAperture, rowBytes * __private->framebufferHeight);
                    if (NULL != fbRange)
                    {
                        IOMemoryMap * newVramMap = fbRange->map(kIOFBMapCacheMode);
                        fbRange->release();
                        if (NULL != newVramMap)
                        {
                            fVramMap->release();
                            fVramMap = newVramMap;

                            fFrameBuffer = (volatile unsigned char *)fVramMap->getVirtualAddress();
                            if (NULL != fFrameBuffer)
                            {
                                frameBufferLength = fVramMap->getLength();
                                if (frameBufferLength < (rowBytes * __private->framebufferHeight))
                                {
                                    // If we end up here even after remap, then the vendor has a driver bug
                                    // They are reporting a memory range that is less than required for the active size & depth.
                                    IOLog("VENDOR_BUG: Invalid aperture range found during restore for: %#llx (%#x:%u:%llu)\n", __private->regID, __private->online, __private->pixelInfo.bytesPerRow * __private->pixelInfo.activeHeight, frameBufferLength);
                                }
                                else
                                {
                                    bPerformVRAMWrite = true;
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                // No vram map?  Should never be the case if fFrameBuffer is valid.
                bPerformVRAMWrite = false;
            }
        }

		if (fFrameBuffer && (true == bPerformVRAMWrite))
		{
            if (kIOScreenRestoreStateDark == restoreType)
			{
                IOG_KTRACE(DBG_IOG_VRAM_BLACK, DBG_FUNC_START,
                           0, __private->regID, 0, 0, 0, 0, 0, 0);
				volatile unsigned char * line = fFrameBuffer;
				for (uint32_t y = 0; y < __private->framebufferHeight; y++)
				{
					bzero((void *) line, __private->framebufferWidth * __private->cursorBytesPerPixel);
					line += rowBytes;
				}
                IOG_KTRACE(DBG_IOG_VRAM_BLACK, DBG_FUNC_END,
                           0, __private->regID, 0, 0, 0, 0, 0, 0);
			}
			else
			{
                IOG_KTRACE(DBG_IOG_VRAM_RESTORE, DBG_FUNC_START,
                           0, __private->regID, 0, 0, 0, 0, 0, 0);
	#if VRAM_COMPRESS
				uint32_t image = (kOSBooleanTrue == 
					IORegistryEntry::getRegistryRoot()->getProperty(kIOConsoleLockedKey));
				if (image >= kIOPreviewImageCount) image = 0;
				DecompressData((UInt8 *) __private->saveFramebuffer, image, (UInt8 *) fFrameBuffer,
								0, 0, __private->framebufferWidth, __private->framebufferHeight, rowBytes);
	#else
				bcopy_nc( __private->saveFramebuffer, (void *) fFrameBuffer, __private->saveLength );
	#endif
                IOG_KTRACE(DBG_IOG_VRAM_RESTORE, DBG_FUNC_END,
                           0, __private->regID, 0, 0, 0, 0, 0, 0);
				DEBG1(thisName, " screen drawn\n");
			}
		}
		DEBG1(thisName, " restoretype %d->%d\n", __private->restoreType, restoreType);
		__private->needGammaRestore = (fFrameBuffer && (0 == __private->restoreType));
		__private->restoreType      = restoreType;

		if (kIOFBNotifyVRAMReady != event)
		{
            FB_START(setAttribute,kIOFBSpeedAttribute,__LINE__,__private->reducedSpeed);
			setAttribute(kIOFBSpeedAttribute, __private->reducedSpeed);
            FB_END(setAttribute,0,__LINE__,0);
			__private->controller->setPowerThread(saveThread);
		}

		ret = kIOReturnSuccess;
	}

	if ((kIOFBNotifyVRAMReady != event) && __private->restoreType && __private->needGammaRestore)
	{
		if (__private->gammaDataLen && __private->gammaData 
		  && !__private->scaledMode
		  && !__private->wakingFromHibernateGfxOn
          && !__private->transactionsEnabled)
		{
			DEBG1(thisName, " set gamma\n");
            FB_START(setGammaTable,0,__LINE__,0);
            IOG_KTRACE(DBG_IOG_SET_GAMMA_TABLE, DBG_FUNC_START,
                       0, __private->regID,
                       0, DBG_IOG_SOURCE_RESTORE_FRAMEBUFFER, 0, 0, 0, 0);
			IOReturn kr = setGammaTable( __private->gammaChannelCount, __private->gammaDataCount,
							__private->gammaDataWidth, __private->gammaData );
            IOG_KTRACE(DBG_IOG_SET_GAMMA_TABLE, DBG_FUNC_END,
                       0, __private->regID,
                       0, DBG_IOG_SOURCE_RESTORE_FRAMEBUFFER,
                       0, kr, 0, 0);
            FB_END(setGammaTable,0,__LINE__,0);
		}
		__private->needGammaRestore = false;
	}
#endif /* VRAM_SAVE */

    IOFB_END(restoreFramebuffer,ret,0,0);
    return (ret);
}

IOReturn IOFramebuffer::handleEvent( IOIndex event, void * info )
{
    IOFB_START(handleEvent,event,0,0);
    IOReturn ret = kIOReturnSuccess;
	bool     sendEvent = true;

    IOG_KTRACE(DBG_IOG_HANDLE_EVENT, DBG_FUNC_NONE,
               0, __private->regID,
               0, event, 0, 0, 0, 0);

    DEBG1(thisName, "(%d, %d)\n", (uint32_t) event,
          !__private->controller->onPowerThread());

    if (!__private->controller->onPowerThread())
    {
		sendEvent = false;
    }
    else switch (event)
    {
        case kIOFBNotifyWillSleep:
            if (!info)
				break;

            if (this == gIOFBConsoleFramebuffer)
            {
//              getPlatform()->setConsoleInfo( 0, kPEDisableScreen);
                killprint = 1;
            }
			__private->restoreType = kIOScreenRestoreStateNone;
			// notification sent early at system sleep
			sendEvent = false;
			ret = kIOReturnSuccess;
            break;

        case kIOFBNotifyDidWake:
            if (!info)
				break;

			restoreFramebuffer(event);

            if (this == gIOFBConsoleFramebuffer)
            {
//              getPlatform()->setConsoleInfo( 0, kPEEnableScreen);
                killprint = 0;
                kmputc( 033 );
                kmputc( 'c' );
            }

            sleepConnectCheck = true;

#if DOANIO
            doanio();
#endif
			pagingState = true;
            FBWL(this)->wakeupGate(&fServerConnect, kManyThreadWakeup); // wakeup is not gate specific
		    __private->actualVBLCount = 0;
			updateVBL(this, NULL);
			ret = deliverFramebufferNotification(kIOFBNotifyDidWake, (void *) true);
			info = (void *) false;
			break;

        case kIOFBNotifyDidPowerOn:
			pagingState = true;
			ret = kIOReturnSuccess;
            break;

        case kIOFBNotifyVRAMReady:
			ret = restoreFramebuffer(event);
			sendEvent = false;
            break;
    }

	if (sendEvent)
		ret = deliverFramebufferNotification(event, info);

    IOFB_END(handleEvent,ret,0,0);
    return (ret);
}

IOReturn IOFramebuffer::notifyServer(const uint8_t state)
{
    IOFB_START(notifyServer,state,0,0);

    SYSASSERTGATED();
    FBGATEGUARD(ctrlgated, this);

    // Convenience aliases to save typing and shorten line-length
    auto& aliasSentPower  = __private->fServerMsgIDSentPower;
    auto& aliasAckedPower = __private->fServerMsgIDAckedPower;

    IOReturn            err = kIOReturnSuccess;
    mach_msg_header_t * msgh = (mach_msg_header_t *) serverMsg;
    msgh->msgh_id = state;

    bool sendMessage = false;
    uint64_t metricsDomain = 0;
    uint64_t sentAckedPower = 0;

    const bool isPower = (state == kIOFBNS_Sleep || state == kIOFBNS_Wake
                      ||  state == kIOFBNS_Doze);
    if (isPower) {
        if ((aliasSentPower & kIOFBNS_MessageMask) != state)
        {
            if (!gChosenEntry)
                gChosenEntry = IORegistryEntry::fromPath("/chosen", gIODTPlane);
            if (gChosenEntry && !state)
                gChosenEntry->removeProperty(kIOScreenLockStateKey);
            if (state)
            {
                if ((this == gIOFBConsoleFramebuffer) || (true == gIOFBSetPreviewImage))
                {
                    getPMRootDomain()->removeProperty(kIOHibernatePreviewBufferKey);
                    getProvider()->removeProperty(kIOHibernatePreviewActiveKey);
                }
                if (__private->saveFramebuffer && __private->saveLength)
                {
                    IOFreePageable( __private->saveFramebuffer, __private->saveLength );
                }
                __private->saveFramebuffer = 0;
                __private->saveLength      = 0;
                setProperty(kIOScreenRestoreStateKey,
                            &__private->restoreType, sizeof(__private->restoreType));
                DEBG1(thisName, " kIOScreenRestoreStateKey %d\n", __private->restoreType);
                __private->restoreType      = kIOScreenRestoreStateNone;
                __private->needGammaRestore = false;
            }

            metricsDomain = kGMETRICS_DOMAIN_FRAMEBUFFER
                          | (state == kIOFBNS_Sleep ? kGMETRICS_DOMAIN_SLEEP
                          : (state == kIOFBNS_Wake  ? kGMETRICS_DOMAIN_WAKE
                          :  kGMETRICS_DOMAIN_DOZE));

            // Retrieve Encode the display state before sending message
            uintptr_t dispState = 0;
            if (__private->fServerUsesModernAcks && kIOFBNS_Wake == state) {
                FB_START(getAttribute,kIOFBDisplayState,__LINE__,0);
                const IOReturn e = getAttribute(kIOFBDisplayState, &dispState);
                FB_END(getAttribute,e,__LINE__,0);
                if (!e) {
                    // TODO(gvdl): Log any error
                    dispState <<= kIOFBNS_DisplayStateShift;
                    dispState &= kIOFBNS_DisplayStateMask;
                }
                msgh->msgh_id |= static_cast<integer_t>(dispState);
            }
            sendMessage = true;
            sentAckedPower = static_cast<uint64_t>(aliasSentPower) << 32
                           | aliasAckedPower;
        }
    }

    uint64_t hidden = 0;

    if (sendMessage) {
        if (__private->fServerUsesModernAcks) {
            // Add generation count
            const uint32_t msgGen
                = (++__private->fServerMsgCount << kIOFBNS_GenerationShift)
                & kIOFBNS_GenerationMask;
            msgh->msgh_id |= msgGen;
            DEBG1(thisName, " sending modern notification %x\n", msgh->msgh_id);
        }

        GMETRICFUNC(
                DBG_IOG_NOTIFY_SERVER, kGMETRICS_EVENT_SIGNAL, metricsDomain);
        IOG_KTRACE(DBG_IOG_NOTIFY_SERVER, DBG_FUNC_NONE,
                   0, __private->regID,
                   0, msgh->msgh_id,
                   0, sentAckedPower,
                   0, hidden);

        if ((MACH_PORT_NULL == msgh->msgh_remote_port)
        ||  (KERN_SUCCESS != mach_msg_send_from_kernel(msgh, msgh->msgh_size)))
            err = kIOReturnIPCError;

        if (isPower) {
            aliasSentPower = msgh->msgh_id;
            if (err) {
                // failed to send, fake an ack
                aliasAckedPower = aliasSentPower;
                __private->fDeferDozeUntilAck = false;
            }
            else {
                // start ack timeout
                __private->fDeferDozeUntilAck
                    = ((state == kIOFBNS_Sleep) && __private->online);
                gIOFBServerAckTimer->
                    setTimeout(kServerAckTimeout, kSecondScale);
            }
            err = kIOReturnSuccess;
            DEBG1(thisName, "(power %p, wait %d, %x->%x)\n",
                    msgh->msgh_remote_port, __private->controller->fWsWait,
                    aliasAckedPower, aliasSentPower);
        }
        else
            assert(false);  // Should never happen, internal error
    }

    if (state == kIOFBNS_Wake) {
        // wakeup is not gate specific
        FBWL(this)->wakeupGate(&fServerConnect, kManyThreadWakeup);
    }

    IOFB_END(notifyServer,err,0,0);
    return (err);
}

bool IOFramebuffer::getIsUsable( void )
{
    IOFB_START(getIsUsable,0,0,0);
    bool b = (dead || (0 != isUsable));
    IOFB_END(getIsUsable,b,0,0);
    return (b);
}

IOReturn IOFramebuffer::postWake(void)
{
    IOFB_START(postWake,0,0,0);
    IOReturn  ret;
    uintptr_t value;
    bool      probeDP;

	DEBG1(thisName, " post wake from sleep %d\n", sleepConnectCheck);

    probeDP = (__private->dpDongle && !sleepConnectCheck);

    FB_START(getAttributeForConnection,kConnectionPostWake,__LINE__,0);
    ret = getAttributeForConnection(0, kConnectionPostWake, &value);
    FB_END(getAttributeForConnection,ret,__LINE__,0);

    if (captured)
		gIOFBProbeCaptured = true;
	else
    {
        FB_START(getAttributeForConnection,kConnectionChanged,__LINE__,0);
        getAttributeForConnection(0, kConnectionChanged, 0);
        FB_END(getAttributeForConnection,0,__LINE__,0);
        if (probeDP)
        {
            FB_START(setAttributeForConnection,kConnectionProbe,__LINE__,kIOFBUserRequestProbe);
#if RLOG
            IOReturn probeErr =
#endif
            setAttributeForConnection(0, kConnectionProbe, kIOFBUserRequestProbe);
            FB_END(setAttributeForConnection,0,__LINE__,0);
            DEBG(thisName, " dp probe wake result %x\n", probeErr);
        }
    }

    __private->controller->fPostWakeChange = __private->controller->fConnectChange;
    if (sleepConnectCheck)
    {
        if (kIOGDbgNoClamshellOffline & atomic_load(&gIOGDebugFlags))
        {
            gIOFBLastReadClamshellState = gIOFBCurrentClamshellState;
        }
        sleepConnectCheck = false;
    }

	resetClamshell(kIOFBClamshellProbeDelayMS, DBG_IOG_SOURCE_POSTWAKE);
    IOG_KTRACE(DBG_IOG_CLAMSHELL, DBG_FUNC_NONE,
               0, DBG_IOG_SOURCE_POSTWAKE,
               0, gIOFBLastClamshellState,
               0, gIOFBLastReadClamshellState,
               0, gIOFBCurrentClamshellState);

    IOFB_END(postWake,ret,0,0);
    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::pmSettingsChange(OSObject * target, const OSSymbol * type,
                                         OSObject * val, uintptr_t refcon)
{
    IOFB_START(pmSettingsChange,0,0,0);
    if (type == gIOFBPMSettingDisplaySleepUsesDimKey)
        triggerEvent(kIOFBEventDisplayDimsSetting);
    IOFB_END(pmSettingsChange,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// system WL

void IOFramebuffer::systemWork(OSObject * owner,
                               IOInterruptEventSource * evtSrc, int intCount)
{
    IOFB_START(systemWork,intCount,0,0);
    uint_fast32_t events;
    IOFBController *    controller;
    IOFramebuffer *     fb;
    uint32_t            allState;
    
	allState = 0;

    STAILQ_FOREACH(controller, &gIOFBAllControllers, fNextController)
        allState |= controller->computeState();

    DEBG1("S1", " state 0x%x\n", allState);
    IOG_KTRACE_NT(DBG_IOG_SYSTEM_WORK, DBG_FUNC_START,
        allState, gIOFBGlobalEvents, 0, 0);

    STAILQ_FOREACH(controller, &gIOFBAllControllers, fNextController)
    {
		if (controller->fNeedsWork)
		{
			uint32_t state = allState;
			DEBG1(controller->fName, " working %x\n", state);

            FCGATEGUARD(ctrlgated, controller);
            if (!controller->fDidWork)
			{
                events = clearEvent(kIOFBEventSystemPowerOn);
				if (events & kIOFBEventSystemPowerOn)
					muxPowerMessage(kIOMessageDeviceWillPowerOn);

				state |= controller->checkPowerWork(state);
			
				if (kIOFBServerSentPowerOff & state)
				{
					if ((controller->fLastFinishedChange != controller->fConnectChange)
							 && !controller->fFbs[0]->messaged
							 && gIOFBIsMuxSwitching)
					{
                        IOG_KTRACE_NT(DBG_IOG_MUX_ACTIVITY_CHANGE,
                            DBG_FUNC_NONE,
                            controller->fFbs[0]->__private->regID, 0, 0, 0);
						IODisplayWrangler::activityChange(controller->fFbs[0]);
					}
				}
				else if (!(kIOFBServerAckedPowerOff & state))
				{
					state |= controller->checkConnectionWork(state);
					controller->fNeedsWork = false;
				}
			}
		}
	}

	const bool nextWSPowerOn = (gIOFBSystemPower || gIOFBIsMuxSwitching)
                            && !(kIOFBDimmed & allState);
	if (nextWSPowerOn != gIOFBCurrentWSPowerOn)
	{
        // Always try to notify a power up, or if the WS has checked in then
        // send notifications for power downs too.
		if (nextWSPowerOn || !(kIOFBWsWait & allState))
		{
			if (nextWSPowerOn && (kIOFBServerAckedPowerOn & allState))
			{
                // Server already knows we are power on, wierd log
				DEBG1("S", " notifyServer wait ack\n");
			}
			else
			{
				DEBG1("S", " notifyServer(%d)\n", nextWSPowerOn);


                // Update our internal state
				gIOFBCurrentWSPowerOn = nextWSPowerOn;
				if (nextWSPowerOn)
					gIOFBPostWakeNeeded = true;
                const uint8_t fbnsState
                    = (nextWSPowerOn) ? kIOFBNS_Wake : kIOFBNS_Sleep;
                FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
					fb->notifyServer(fbnsState);
			}
		}
	}

	if (!(kIOFBServerAckedPowerOn & allState) && gIOFBSystemPowerAckTo
		&& !(kIOFBDisplaysChanging & allState)
		&& !gIOFBIsMuxSwitching)
	{
		uintptr_t notiArg;
		// tell accelerators to disable, then protect HW
		for(notiArg = 0; notiArg < 2; notiArg++)
		{
            FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
			{
                FBGATEGUARD(ctrlgated, fb);
				fb->deliverFramebufferNotification(kIOFBNotifyWillSleep, (void *) notiArg);
				if (notiArg)
				{
					fb->saveFramebuffer();
					fb->pagingState = false;
				}
			}
		}
		
		DEBG("S", " allowPowerChange(%p)\n", OBFUSCATE(gIOFBSystemPowerAckRef));
		
		IOService * ackTo  = gIOFBSystemPowerAckTo;
		void *      ackRef = gIOFBSystemPowerAckRef;
		gIOFBSystemPowerAckTo = 0;

        GMETRICFUNC(DBG_IOG_ALLOW_POWER_CHANGE, kGMETRICS_EVENT_SIGNAL,
                   kGMETRICS_DOMAIN_FRAMEBUFFER | kGMETRICS_DOMAIN_POWER);
        IOG_KTRACE(DBG_IOG_ALLOW_POWER_CHANGE, DBG_FUNC_NONE,
                   0, 0, 0, 0, 0, 0, 0, 0);

		ackTo->allowPowerChange( (uintptr_t) ackRef );
		
		DEBG("S", " did allowPowerChange()\n");
	}

	if (!(kIOFBServerAckedPowerOff & allState) && gIOFBPostWakeNeeded)
	{
		gIOFBPostWakeNeeded = false;
        FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
		{
            FBGATEGUARD(ctrlgated, fb);
			fb->postWake();
		}
	}

	events = atomic_load(&gIOFBGlobalEvents);

	if (kIOFBEventCaptureSetting & events)
	{
		bool wasCaptured, wasDimDisable;
		bool newCaptured, newDimDisable;

        clearEvent(kIOFBEventCaptureSetting);

		wasCaptured   = (0 != (kIOFBCaptured & allState));
		wasDimDisable = (0 != (kIOFBDimDisable & allState));
		newCaptured   = (0 != (kIOCaptureDisableDisplayChange  & gIOFBCaptureState));
		newDimDisable = (0 != (kIOCaptureDisableDisplayDimming & gIOFBCaptureState));

        FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
		{
            FBGATEGUARD(ctrlgated, fb);
			fb->setCaptured(newCaptured);
			fb->setDimDisable(newDimDisable);
		}
		if (wasCaptured != newCaptured) gIOFBGrayValue = newCaptured ? 0 : kIOFBGrayValue;

		if (newDimDisable != wasDimDisable)
		{
			getPMRootDomain()->setAggressiveness(kIOFBCaptureAggressiveness, newDimDisable);
		}

		if (gIOFBProbeCaptured && wasCaptured && !newCaptured)
		{
			gIOFBProbeCaptured = false;
            triggerEvent(kIOFBEventProbeAll);
			DEBG1("S", " capt probe all\n");
		}
	}

	if (kIOFBEventDisplayDimsSetting & events)
	{
		OSNumber *  num;
		uintptr_t   value = true;
		OSObject *  obj;
		
        clearEvent(kIOFBEventDisplayDimsSetting);
		obj = getPMRootDomain()->copyPMSetting(const_cast<OSSymbol *>
											   (gIOFBPMSettingDisplaySleepUsesDimKey));
		if ((num = OSDynamicCast(OSNumber, obj)))
			value = num->unsigned32BitValue();
		if (obj)
			obj->release();
		
		if (num) FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
		{
            FBGATEGUARD(ctrlgated, fb);
			fb->deliverFramebufferNotification(kIOFBNotifyDisplayDimsChange, (void *) value);
		}
	}


	if ((kIOFBEventReadClamshell & events) 
    && !gIOFBIsMuxSwitching && !(kIOFBWsWait & allState))
	{
		OSObject * clamshellProperty;

        clearEvent(kIOFBEventReadClamshell);
		clamshellProperty = gIOResourcesAppleClamshellState;
		if (clamshellProperty)
		{
			gIOFBLastClamshellState = gIOFBCurrentClamshellState
                = (kOSBooleanTrue == clamshellProperty);
			DEBG1("S", " clamshell read %d\n", (int) gIOFBCurrentClamshellState);
            FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
			{
                FBGATEGUARD(ctrlgated, fb);
				fb->deliverFramebufferNotification(kIOFBNotifyClamshellChange, clamshellProperty);
			}

            bool openTest;
            if (kIOGDbgNoClamshellOffline & atomic_load(&gIOGDebugFlags))
            {
                openTest = gIOFBLidOpenMode
                    ? gIOFBDisplayCount > 0 : gIOFBBacklightDisplayCount <= 0;
            }
            else
            {
                openTest = gIOFBLidOpenMode || gIOFBBacklightDisplayCount <= 0;
            }
            const bool desktopMode = gIOFBDesktopModeAllowed && openTest;
            if (desktopMode)
            {
                // lid change, desktop mode
                DEBG1("S", " desktop will reprobe\n");
                resetClamshell(kIOFBClamshellProbeDelayMS,
                               DBG_IOG_SOURCE_SYSWORK_READCLAMSHELL);
            }

            IOG_KTRACE(DBG_IOG_CLAMSHELL, DBG_FUNC_NONE,
                       0, DBG_IOG_SOURCE_SYSWORK_READCLAMSHELL,
                       0, kOSBooleanTrue == clamshellProperty,
                       0, gIOFBCurrentClamshellState,
                       0, desktopMode);
		}
        else {
            IOG_KTRACE(DBG_IOG_CLAMSHELL, DBG_FUNC_NONE,
                       0, DBG_IOG_SOURCE_SYSWORK_READCLAMSHELL,
                       0, -1, 0, 0, 0, 0);
        }
	}

	if ((kIOFBEventResetClamshell & events) && gIOFBSystemPower
    && !(kIOFBWsWait & allState)
    && !(kIOFBDisplaysChanging & allState))
	{
        clearEvent(kIOFBEventResetClamshell);

		DEBG1("S", " kIOFBEventResetClamshell %d, %d\n", gIOFBCurrentClamshellState, gIOFBLastReadClamshellState);

        const bool hasLidChanged =
            (gIOFBCurrentClamshellState != gIOFBLastReadClamshellState);
        const bool hasExtDisplayChangedWhileClosed =
            ((0 == gIOFBLastDisplayCount) != (0 == gIOFBDisplayCount))
            && gIOFBCurrentClamshellState;
        const bool modernReprobe =
            (hasLidChanged || hasExtDisplayChangedWhileClosed)
            && gIOFBLidOpenMode;
        const bool legacyReprobe = gIOFBBacklightDisplayCount
            && !gIOFBCurrentClamshellState && gIOFBLastReadClamshellState
            && !gIOFBLidOpenMode;
        const bool probeNow = modernReprobe || legacyReprobe;
		if (probeNow)
		{
			DEBG1("S", " clamshell caused reprobe\n");
			events |= kIOFBEventProbeAll;
            atomic_fetch_or(&gIOFBGlobalEvents, kIOFBEventProbeAll);
		}
		else
		{
            // <rdar://problem/30168104> Disconnecting DisplayPort Adapter isn't locking the MacBook
            // Removing the 15 second delay introduced:
            // <rdar://problem/31205784> J80a: Internal panel doesn't light when opening lid after rebooting in closed clamshell with NON-DDC VGA
            // <rdar://problem/30312042> Underrun on booting with J130/J130a with closed clamshell and T240
            // <rdar://problem/31367833> J79a/T240: screen keep black when boot up under closed clamshell mode
            // <rdar://problem/31410643> IG IOFB crashes on Wake on J130A/16F46 when booted in Clamshell mode with T240 connected
            // <rdar://problem/31413646> J130/Evolution: Unit doesn't boot to desktop on closed clamshell (with T240) reboot intermittently, goes to sleep in boot process
            // <rdar://problem/31351644> [Reg]Unable to light up T240 in Fletcher 2047 in closed clamshell
            // Therefore reverted back to the original implementation:
            AbsoluteTime deadline;
            clock_interval_to_deadline(kIOFBClamshellEnableDelayMS, kMillisecondScale, &deadline );
            thread_call_enter1_delayed(gIOFBClamshellCallout,
                                       (thread_call_param_t) kIOFBEventEnableClamshell, deadline );
		}

        const uint64_t bits =
            (gIOFBCurrentClamshellState          ? (1ULL << 0) : 0) |
            (gIOFBLastReadClamshellState         ? (1ULL << 1) : 0) |
            (probeNow                            ? (1ULL << 2) : 0) |
            (gIOFBLidOpenMode                    ? (1ULL << 3) : 0) |
            (hasExtDisplayChangedWhileClosed     ? (1ULL << 4) : 0) |
            (hasLidChanged                       ? (1ULL << 5) : 0) |
            0;
        const uint64_t counts =
            GPACKUINT8T(2, gIOFBBacklightDisplayCount) |
            GPACKUINT8T(1, gIOFBDisplayCount) |
            GPACKUINT8T(0, gIOFBLastDisplayCount) |
            0;
        IOG_KTRACE(DBG_IOG_CLAMSHELL, DBG_FUNC_NONE,
                   0, DBG_IOG_SOURCE_SYSWORK_RESETCLAMSHELL_V2,
                   0, bits, 0, counts, 0, 0);
	}

	if ((kIOFBEventEnableClamshell & events) && gIOFBSystemPower
    && !(kIOFBWsWait & allState)
    && !(kIOFBDisplaysChanging & allState))
	{
		UInt32      change;
		bool        desktopMode;

        clearEvent(kIOFBEventEnableClamshell);

		if (gIOFBLidOpenMode)
			desktopMode = gIOFBDesktopModeAllowed && (gIOFBDisplayCount > 0);
		else
			desktopMode = gIOFBDesktopModeAllowed && (gIOFBBacklightDisplayCount <= 0);

		change = kIOPMEnableClamshell | kIOPMSetDesktopMode | (desktopMode ? kIOPMSetValue : 0);
		if (change != gIOFBClamshellState)
		{
			gIOFBClamshellState = change;
			DEBG1("S", " clamshell ena desktopMode %d bldisp %d disp %d\n",
				desktopMode, gIOFBBacklightDisplayCount, gIOFBDisplayCount);

            IOG_KTRACE(DBG_IOG_RECEIVE_POWER_NOTIFICATION, DBG_FUNC_NONE,
                       0, DBG_IOG_PWR_EVENT_DESKTOPMODE,
                       0, gIOFBClamshellState, 0, 0, 0, 0);

			getPMRootDomain()->receivePowerNotification(change);
		}
        IOG_KTRACE(DBG_IOG_CLAMSHELL, DBG_FUNC_NONE,
                   0, DBG_IOG_SOURCE_SYSWORK_ENABLECLAMSHELL,
                   0, desktopMode,
                   0, change, 0, 0);
	}

	if (kIOFBEventDisplaysPowerState & events)
	{
		unsigned long state = IODisplayWrangler::getDisplaysPowerState();

		if (gIOFBIsMuxSwitching && (state < kIODisplayNumPowerStates))
		{
			state = kIODisplayNumPowerStates;
		}
		else
		{
            clearEvent(kIOFBEventDisplaysPowerState);
		}
		DEBG1("S", " displays pstate %ld\n", state);

        FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
		{
            FBGATEGUARD(ctrlgated, fb);
			if (fb->__private->display)
				fb->__private->display->setDisplayPowerState(state);
		}
	}

	if ((kIOFBEventProbeAll & events) 
		&& gIOFBSystemPower 
		&& (kIOMessageSystemHasPoweredOn == gIOFBLastMuxMessage)
		&& !(kIOFBWsWait & allState)
		&& !(kIOFBCaptured & allState)
		&& !(kIOFBDisplaysChanging & allState)
        && !gIOFBIsMuxSwitching)
	{
        clearEvent(kIOFBEventProbeAll);

        IOG_KTRACE(DBG_IOG_CLAMSHELL, DBG_FUNC_NONE,
                   0, DBG_IOG_SOURCE_SYSWORK_PROBECLAMSHELL,
                   0, gIOFBLastClamshellState,
                   0, gIOFBLastReadClamshellState,
                   0, gIOFBCurrentClamshellState);
		DEBG1("S", " probeAll clam %d -> %d\n",
				(int) gIOFBLastClamshellState, (int) gIOFBCurrentClamshellState);

		gIOFBLastClamshellState = gIOFBCurrentClamshellState;
		gIOFBLastReadClamshellState = gIOFBCurrentClamshellState;
        gIOFBLastDisplayCount = gIOFBDisplayCount;

		probeAll(kIOFBUserRequestProbe);
        FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
		{
            FBGATEGUARD(ctrlgated, fb);
			fb->deliverFramebufferNotification(kIOFBNotifyProbed, NULL);
		}
		resetClamshell(kIOFBClamshellProbeDelayMS,
                       DBG_IOG_SOURCE_SYSWORK_PROBECLAMSHELL);
	}

	if (kIOFBEventVBLMultiplier & events)
	{
        clearEvent(kIOFBEventVBLMultiplier);
        FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
		{
            FBGATEGUARD(ctrlgated, fb);
			fb->setVBLTiming();
		}
	}


    IOG_KTRACE_NT(DBG_IOG_SYSTEM_WORK, DBG_FUNC_END,
        allState, gIOFBGlobalEvents, 0, 0);
    IOFB_END(systemWork,0,0,0);
}

bool IOFramebuffer::copyDisplayConfig(IOFramebuffer *from)
{
    IOFB_START(copyDisplayConfig,0,0,0);
    FBASSERTGATED(this);
    FBASSERTGATED(from);

    DEBG1(from->thisName, " %d x %d\n",
        (int) from->__private->pixelInfo.activeWidth,
        (int) from->__private->pixelInfo.activeHeight);

    __private->pixelInfo    = from->__private->pixelInfo;
    __private->timingInfo   = from->__private->timingInfo;
    __private->uiScale      = from->__private->uiScale;
    __private->matchedMode  = from->__private->setupMode;

    if (from->__private->displayAttributes) from->__private->displayAttributes->retain();
    if (__private->displayAttributes) __private->displayAttributes->release();
    __private->displayAttributes =    from->__private->displayAttributes;

    // Hibernate gamma data - fixed size, so this should never fail, but in case
    // the future supports differing sizes, handle this now.
    if (from->__private->hibernateGammaDataLen != __private->hibernateGammaDataLen)
    {
        if (__private->hibernateGammaDataLen)
        {
            IODelete(__private->hibernateGammaData, uint8_t, __private->hibernateGammaDataLen);
        }
        __private->hibernateGammaData = IONew(uint8_t, from->__private->hibernateGammaDataLen);
    }
    if (NULL == __private->hibernateGammaData)
    {
        __private->hibernateGammaDataLen      = 0;
        __private->hibernateGammaChannelCount = 0;
        __private->hibernateGammaDataCount    = 0;
        __private->hibernateGammaDataWidth    = 0;
    }
    else
    {
        __private->hibernateGammaDataLen      = from->__private->hibernateGammaDataLen;
        __private->hibernateGammaChannelCount = from->__private->hibernateGammaChannelCount;
        __private->hibernateGammaDataCount    = from->__private->hibernateGammaDataCount;
        __private->hibernateGammaDataWidth    = from->__private->hibernateGammaDataWidth;

        bcopy(from->__private->hibernateGammaData, __private->hibernateGammaData,
              __private->hibernateGammaDataLen);
    }

    if (from->__private->rawGammaDataLen != __private->rawGammaDataLen)
    {
        if (__private->rawGammaDataLen)
            IODelete(__private->rawGammaData, UInt8, __private->rawGammaDataLen);
        __private->rawGammaData = IONew(UInt8, from->__private->rawGammaDataLen);
    }
    if (!__private->rawGammaData)
    {
        __private->rawGammaDataLen = 0;
        IOFB_END(copyDisplayConfig,false,0,0);
        return false;
    }
    __private->rawGammaDataLen      = from->__private->rawGammaDataLen;
    __private->rawGammaChannelCount = from->__private->rawGammaChannelCount;
    __private->rawGammaDataCount    = from->__private->rawGammaDataCount;
    __private->rawGammaDataWidth    = from->__private->rawGammaDataWidth;

    bcopy(from->__private->rawGammaData, __private->rawGammaData,
            __private->rawGammaDataLen);
    IOFB_END(copyDisplayConfig,true,0,0);
    return true;
}

void IOFramebuffer::
serverPowerTimeout(OSObject * /* target */, IOTimerEventSource * /* sender */)
{
    IOFB_START(serverPowerTimeout,0,0,0);
    SYSASSERTGATED();  // Run from syswork timer event source

    IOFramebuffer * fb;
    FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
	{
        FBGATEGUARD(ctrlgated, fb);
        // Alias for readability
        const auto& sentServerPower = fb->__private->fServerMsgIDSentPower;
        const auto& serverAckedPower = fb->__private->fServerMsgIDAckedPower;
		if (serverAckedPower != sentServerPower)
		{
			DEBG1(fb->thisName, " (%x->%x) server ack timeout\n",
                  serverAckedPower, sentServerPower);

            const uint64_t powerState = GPACKUINT32T(0, serverAckedPower)
                                      | GPACKUINT32T(1, sentServerPower);
            (void) powerState;
            IOG_KTRACE(DBG_IOG_SERVER_TIMEOUT, DBG_FUNC_NONE,
                       0, fb->__private->regID,
                       0, DBG_IOG_SOURCE_SERVER_ACK_TIMEOUT,
                       0, powerState,
                       0, fb->__private->fServerMsgCount);
			fb->serverAcknowledgeNotification(sentServerPower);
		}
	}
    IOFB_END(serverPowerTimeout,0,0,0);
}


/* static */ void IOFramebuffer::startThread(bool highPri)
{
    IOFB_START(startThread,highPri,0,0);
    if (gIOFBWorkES)
        gIOFBWorkES->interruptOccurred(0, 0, 0);
    IOFB_END(startThread,0,0,0);
}

// controller WL

bool IOFramebuffer::isWakingFromHibernateGfxOn(void)
{
    IOFB_START(isWakingFromHibernateGfxOn,0,0,0);
	bool b = (__private->wakingFromHibernateGfxOn);
    IOFB_END(isWakingFromHibernateGfxOn,b,0,0);
    return (b);
}

IOOptionBits IOFramebuffer::checkPowerWork(IOOptionBits state)
{
    IOFB_START(checkPowerWork,state,0,0);
    IOOptionBits ourState = kIOFBPaging;
	IOService *  device = 0;
    OSData *     stateData = 0;

	if (pendingPowerChange && !__private->fDeferDozeUntilAck)
	{
		uintptr_t newState = pendingPowerState;

		DEBG1(thisName, " pendingPowerState(%ld)\n", newState);

        thread_t saveThread = __private->controller->setPowerThread();
		__private->hibernateGfxStatus = 0;
		__private->wakingFromHibernateGfxOn = false;
		if (!pagingState)
		{
			device = __private->controller->fDevice;
			if (device 
			  && (stateData = OSDynamicCast(OSData, getPMRootDomain()->getProperty(kIOHibernateStateKey))))
			{
				if (kIOHibernateStateWakingFromHibernate == ((uint32_t *) stateData->getBytesNoCopy())[0])
				{
					OSNumber *
					num = OSDynamicCast(OSNumber, getPMRootDomain()->getProperty(kIOHibernateOptionsKey));
					uint32_t options = 0;
					if (num) options = num->unsigned32BitValue();
					DEBG1(thisName, " kIOHibernateOptionsKey %p (0x%x)\n", OBFUSCATE(num), options);
					if (kIOHibernateOptionDarkWake & options)
					{
						stateData = 0;
					}
					else
					{
						device->setProperty(kIOHibernateStateKey, stateData);
						__private->wakingFromHibernateGfxOn = true;

						OSData * 
						data = OSDynamicCast(OSData, getPMRootDomain()->getProperty(kIOHibernateGfxStatusKey));
						if (data)
						{
							__private->hibernateGfxStatus = ((uint32_t *)data->getBytesNoCopy())[0];
							device->setProperty(kIOHibernateEFIGfxStatusKey, data);
						}
						if (newState == 1) newState = 2;
					}
				}
			}
		}

		// online-online test
		// if (newState && __private->controllerIndex) connectChangeInterrupt(this, 0);

		if ((newState == 1) 
			&& (kIODisplayOptionDimDisable & __private->displayOptions) 
// 9564372			&& __private->audioStreaming
			&& !gIOFBSystemDark)
		{
			newState = 2;
		}

        const uint64_t setMetricFunc = DBG_IOG_SET_POWER_ATTRIBUTE;
        const uint64_t setMetricDomain
            = kGMETRICS_DOMAIN_FRAMEBUFFER | kGMETRICS_DOMAIN_VENDOR
            | kGMETRICS_DOMAIN_POWER
            | GMETRIC_DOMAIN_FROM_POWER_STATE(newState, getPowerState());
        FB_START(setAttribute,kIOPowerStateAttribute,__LINE__,newState);
        GMETRICFUNC(setMetricFunc, kGMETRICS_EVENT_START, setMetricDomain);
        IOG_KTRACE(DBG_IOG_SET_POWER_ATTRIBUTE, DBG_FUNC_START,
                   0, __private->regID,
                   0, newState, 0, 0, 0, 0);
		setAttribute(kIOPowerStateAttribute, newState);
        IOG_KTRACE(DBG_IOG_SET_POWER_ATTRIBUTE, DBG_FUNC_END,
                   0, __private->regID,
                   0, newState, 0, 0, 0, 0);
        GMETRICFUNC(setMetricFunc, kGMETRICS_EVENT_END, setMetricDomain);
        FB_END(setAttribute,0,__LINE__,0);
        DEBG1(thisName, " did kIOPowerStateAttribute(%ld)\n", newState);
		if (device && stateData)
		{
			device->setProperty(kIOHibernateStateKey, gIOFBZero32Data);
			device->removeProperty(kIOHibernateEFIGfxStatusKey);
		}

        __private->controller->setPowerThread(saveThread);

		OSObject * obj;
        if (((this == gIOFBConsoleFramebuffer) || (true == gIOFBSetPreviewImage))
			&& (obj = copyProperty(kIOHibernatePreviewActiveKey, gIOServicePlane)))
		{
			getPMRootDomain()->setProperty(kIOHibernatePreviewActiveKey, obj);
			obj->release();
		}

		DEBG(thisName, " acknowledgeSetPowerState\n");
		pendingPowerChange = false;

        const uint64_t ackMetricFunc
            = GMETRIC_DATA_FROM_MARKER(
                    GMETRIC_MARKER_FROM_POWER_STATE(newState, getPowerState()))
            | GMETRIC_DATA_FROM_FUNC(DBG_IOG_ACK_POWER_STATE);
        const uint64_t ackMetricDomain
            = kGMETRICS_DOMAIN_FRAMEBUFFER | kGMETRICS_DOMAIN_POWER
            | GMETRIC_DOMAIN_FROM_POWER_STATE(newState, getPowerState());
        GMETRIC(ackMetricFunc, kGMETRICS_EVENT_SIGNAL, ackMetricDomain);
        // /!\ Not entirely accurate since each framebuffer has its own power state
        gIOFBPowerState = newState;
        IOG_KTRACE(DBG_IOG_ACK_POWER_STATE, DBG_FUNC_NONE,
                   0, DBG_IOG_SOURCE_CHECK_POWER_WORK,
                   0, __private->regID,
                   0, newState,
                   0, 0);
		acknowledgeSetPowerState();
	}

    if (__private->allowSpeedChanges && __private->pendingSpeedChange)
    {
        thread_t saveThread = __private->controller->setPowerThread();
        __private->pendingSpeedChange = false;
        FB_START(setAttribute,kIOFBSpeedAttribute,__LINE__,__private->reducedSpeed);
        setAttribute(kIOFBSpeedAttribute, __private->reducedSpeed);
        FB_END(setAttribute,0,__LINE__,0);
        __private->controller->setPowerThread(saveThread);
    }

    IOFB_END(checkPowerWork,ourState,0,0);
    return (ourState);
}

IOReturn IOFramebuffer::extEndConnectionChange(void)
{
    IOFB_START(extEndConnectionChange,0,0,0);
    IOFBController * controller = __private->controller;
    SYSASSERTGATED();
    FBASSERTGATED(this);

	DEBG1(controller->fName, " WS done msg %d, onl %x, count(%d, msg %d, fin %d, proc %d, wake %d)\n",
		  controller->fFbs[0]->messaged, controller->fOnlineMask,
          controller->fConnectChange,
          controller->fLastMessagedChange, controller->fLastFinishedChange,
          controller->fFbs[0]->__private->lastProcessedChange,
          controller->fPostWakeChange);

    const auto c1 =
        GPACKBIT(0, controller->fFbs[0]->messaged) |
        GPACKUINT32T(1, controller->fOnlineMask);
    const auto c2 =
        GPACKUINT8T(0, controller->fConnectChange) |
        GPACKUINT8T(1, controller->fLastMessagedChange) |
        GPACKUINT8T(2, controller->fLastFinishedChange) |
        GPACKUINT8T(3, controller->fPostWakeChange) |
        GPACKUINT8T(4, controller->fFbs[0]->__private->lastProcessedChange);
    IOG_KTRACE_NT(DBG_IOG_EXT_END_CONNECT_CHANGE, DBG_FUNC_NONE,
        controller->fFbs[0]->__private->regID, c1, c2, 0);

    if (!controller->fFbs[0]->messaged)
    {
        IOFB_END(extEndConnectionChange,kIOReturnSuccess,__LINE__,0);
        return (kIOReturnSuccess);
    }

	controller->fFbs[0]->messaged = false;
	controller->fLastFinishedChange = controller->fLastMessagedChange;

    if (gIOGraphicsControl)
    {
        IOReturn err;
        err = gIOGraphicsControl->message(kIOFBMessageEndConnectChange, controller->fFbs[0], NULL);
        if (gIOFBIsMuxSwitching && !controller->fOnlineMask &&
            controller->fAliasID && controller->isMuted())
        {
            controller->fMuxNeedsBgOff = true;
        }
    }

	resetClamshell(kIOFBClamshellProbeDelayMS,
                   DBG_IOG_SOURCE_END_CONNECTION_CHANGE);
	controller->startThread();

    IOFB_END(extEndConnectionChange,kIOReturnSuccess,0,0);
	return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::extProcessConnectionChange(void)
{
    IOFB_START(extProcessConnectionChange,0,0,0);
    IOFBController * controller = __private->controller;
    IOReturn         err = kIOReturnSuccess;
    IOOptionBits     mode = 0;

	if ((err = extEntrySys(true, kIOGReportAPIState_ProcessConnectionChange)))
	{
        IOFB_END(extProcessConnectionChange,err,__LINE__,0);
		return (err);
	}

    const auto msgd = controller->fFbs[0] && controller->fFbs[0]->messaged;
    const auto a1 =
        GPACKBIT(0, msgd) |
        GPACKBIT(1, controller->isMuted());
    const auto a2 =
        GPACKUINT32T(0, __private->lastProcessedChange) |
        GPACKUINT32T(1, controller->fConnectChange);
    const auto a3 =
        GPACKUINT32T(0, __private->aliasMode);
    IOG_KTRACE_NT(DBG_IOG_EXT_PROCESS_CONNECT_CHANGE, DBG_FUNC_START,
        __private->regID, a1, a2, a3);
    if (msgd)
	{
		if (controller->isMuted())
			mode = fgOff;
		else if (__private->lastProcessedChange != controller->fConnectChange)
			mode = fg;
		else if (kIODisplayModeIDInvalid != __private->aliasMode)
		{
			bool			rematch = false;

			IOFramebuffer  *oldfb;
			IOFBController *oldController = controller->alias(__FUNCTION__);
            {
                FCGATEGUARD(oldctrlgated, oldController);

                oldfb = oldController->fFbs[__private->controllerIndex];
                if (oldfb)
                {
                    DEBG1(thisName, " check copy from %s: 0x%x==0x%x?\n",
                            oldfb->thisName, (int) __private->matchedMode,
                            (int) oldfb->__private->setupMode);
                    rematch
                        = (__private->matchedMode != oldfb->__private->setupMode);
                    if (rematch)
                    {
                        DEBG1(thisName, " rematch using %d x %d\n",
                            (int) oldfb->__private->pixelInfo.activeWidth,
                            (int) oldfb->__private->pixelInfo.activeHeight);
                        __private->pixelInfo    = oldfb->__private->pixelInfo;
                        __private->timingInfo   = oldfb->__private->timingInfo;
                        __private->uiScale      = oldfb->__private->uiScale;
                        __private->matchedMode  = oldfb->__private->setupMode;
                    }
                }
            }
			if (rematch)
			{
				suspend(true);
				matchFramebuffer();
				suspend(false);
			}
		}
	}

    if (mode)
    {
        IOG_KTRACE(DBG_IOG_CLAMP_POWER_ON, DBG_FUNC_NONE,
                   0, DBG_IOG_SOURCE_PROCESS_CONNECTION_CHANGE,
                   0, 0, 0, 0, 0, 0);

        temporaryPowerClampOn();

        // Drop locks for synchronous terminate. rdar://34506447
        extExitSys(err, kIOGReportAPIState_ProcessConnectionChange);
        IODisplayWrangler::destroyDisplayConnects(this);
        if ((err = extEntrySys(true, kIOGReportAPIState_ProcessConnectionChange)))
        {
            IOFB_END(extProcessConnectionChange,err,__LINE__,0);
            return (err);
        }

        clearEvent(kIOFBEventEnableClamshell);
        if ((kIOPMDisableClamshell != gIOFBClamshellState) &&
            !gIOFBIsMuxSwitching)
        {
            gIOFBClamshellState = kIOPMDisableClamshell;
            IOG_KTRACE(DBG_IOG_RECEIVE_POWER_NOTIFICATION, DBG_FUNC_NONE,
                       0, DBG_IOG_PWR_EVENT_PROCCONNECTCHANGE,
                       0, kIOPMDisableClamshell, 0, 0, 0, 0);
            getPMRootDomain()->receivePowerNotification(kIOPMDisableClamshell);
            DEBG1("S", " clamshell disable\n");
        }

        err = processConnectChange(mode);
    }

    IOG_KTRACE_NT(DBG_IOG_EXT_PROCESS_CONNECT_CHANGE, DBG_FUNC_END,
        __private->regID, mode, err, 0);

    extExitSys(err, kIOGReportAPIState_ProcessConnectionChange);

    IOFB_END(extProcessConnectionChange,err,0,0);
    return (err);
}


IOReturn IOFramebuffer::processConnectChange(IOOptionBits mode)
{
    IOFB_START(processConnectChange,mode,0,0);
    IOReturn  err;
    uintptr_t unused;
    bool      nowOnline;

    IOG_KTRACE_NT(DBG_IOG_PROCESS_CONNECT_CHANGE, DBG_FUNC_START,
        __private->regID, mode, 0, 0);

    DEBG1(thisName, " (%d==%s) curr %d\n", 
        (uint32_t) mode, processConnectChangeModeNames[mode],
        __private->lastProcessedChange);

    if (fgOff == mode)
    {
        displaysOnline(false);
        __private->online = false;
        IOFB_END(processConnectChange,kIOReturnSuccess,__LINE__,0);
        IOG_KTRACE_NT(DBG_IOG_PROCESS_CONNECT_CHANGE, DBG_FUNC_END,
            __private->regID, 1, __private->online, 0);
        return kIOReturnSuccess;
    }
    if (__private->lastProcessedChange == __private->controller->fConnectChange)
    {
        IOFB_END(processConnectChange,kIOReturnSuccess,__LINE__,0);
        IOG_KTRACE_NT(DBG_IOG_PROCESS_CONNECT_CHANGE, DBG_FUNC_END,
            __private->regID, 2, __private->online, 0);
        return kIOReturnSuccess;
    }
    
    if (fg == mode) suspend(true);
    
	{
		// connect change vars here
		__private->enableScalerUnderscan = false;
		__private->audioStreaming        = false;
		__private->colorModesSupported   = 0;
	}
    
    TIMESTART();
    FB_START(getAttributeForConnection,kConnectionChanged,__LINE__,0);
    err = getAttributeForConnection(0, kConnectionChanged, &unused);
    FB_END(getAttributeForConnection,err,__LINE__,0);
    TIMEEND(thisName, "kConnectionChanged time: %qd ms\n");
    
    __private->lastProcessedChange = __private->controller->fConnectChange;
    extSetMirrorOne(0, 0);
    if (fg == mode) suspend(true);

    nowOnline = updateOnline();

    displaysOnline(false);
    __private->online = nowOnline;
    __private->transform = __private->selectedTransform;
    setProperty(kIOFBTransformKey, __private->transform, 64);
    if (nowOnline)
    {
        displaysOnline(true);

        if (__private->cursorAttributes && !__private->cursorAttributes->getCount())
        {
            __private->cursorAttributes->release();
            __private->cursorAttributes = 0;
        }
        
        if (!__private->cursorAttributes)
        {
            if ((__private->cursorAttributes = OSArray::withCapacity(2)))
            {
                __private->testingCursor = true;
                FB_START(setCursorImage,0,__LINE__,0);
                setCursorImage( (void *) 0 );
                FB_END(setCursorImage,0,__LINE__,0);
                __private->testingCursor = false;
                
                setProperty( kIOFBCursorInfoKey, __private->cursorAttributes );
            }
        }
    }
	else
	{
		if (kIODisplayModeIDInvalid != __private->offlineMode)
		{
			IOPixelInformation pixelInfo;

			pixelInfo.bitsPerPixel     = 32;
			pixelInfo.pixelType        = kIORGBDirectPixels;
			pixelInfo.componentCount   = 3;
			pixelInfo.bitsPerComponent = 8;
			__private->currentDepth    = closestDepth(__private->offlineMode, &pixelInfo);

            resetLimitState();
            sendLimitState(__LINE__);

            FB_START(setDisplayMode,0,__LINE__,0);
            IOG_KTRACE(DBG_IOG_SET_DISPLAY_MODE, DBG_FUNC_START,
                       0, DBG_IOG_SOURCE_PROCESS_CONNECTION_CHANGE,
                       0, __private->regID,
                       0, __private->offlineMode,
                       0, __private->currentDepth);
            err = setDisplayMode(__private->offlineMode, __private->currentDepth);
            IOG_KTRACE(DBG_IOG_SET_DISPLAY_MODE, DBG_FUNC_END,
                       0, DBG_IOG_SOURCE_PROCESS_CONNECTION_CHANGE,
                       0, __private->regID,
                       0, err, 0, 0);
            FB_END(setDisplayMode,err,__LINE__,0);
            if (kIOReturnSuccess == err)
            {
                __private->lastSuccessfulMode = __private->offlineMode;
            }
			DEBG1(thisName, " offline setDisplayMode(0x%x, %d) err %x msg %d susp %d\n",
					(int32_t) __private->offlineMode, (int32_t) __private->currentDepth,
					err, messaged, suspended);
		}
		if (fg == mode) suspend(false);
	}

    err = kIOReturnSuccess;

    IOG_KTRACE_NT(DBG_IOG_PROCESS_CONNECT_CHANGE, DBG_FUNC_END,
        __private->regID, 0, __private->online, 0);

    IOFB_END(processConnectChange,err,0,0);
    return (err);
}

bool IOFramebuffer::updateOnline(void)
{
    IOFB_START(updateOnline,0,0,0);
    IOReturn                    err;
    uintptr_t                   connectEnabled;
    bool                        nowOnline;
    IOTimingInformation         info;
    IODisplayModeID *           modeIDs;
    IOItemCount                 modeCount, modeNum;

    TIMESTART();
    FB_START(getAttributeForConnection,kConnectionCheckEnable,__LINE__,0);
    err = getAttributeForConnection(0, kConnectionCheckEnable, &connectEnabled);
    IOG_KTRACE(DBG_IOG_CONNECTION_ENABLE_CHECK, DBG_FUNC_NONE,
               0, DBG_IOG_SOURCE_UPDATE_ONLINE,
               0, __private->regID,
               0, connectEnabled,
               0, err);
    FB_END(getAttributeForConnection,err,__LINE__,connectEnabled);
    TIMEEND(thisName, "kConnectionCheckEnable == %ld, status: %#x, time: %qd ms\n", connectEnabled, err);

    nowOnline = (!dead && ((kIOReturnSuccess != err) || connectEnabled));

    if (nowOnline && __private->bClamshellOffline)
    {
        D(GENERAL, thisName, " forced offline\n");
        nowOnline = false;
    }

    __private->gammaScale[0] = __private->gammaScale[1] 
    	= __private->gammaScale[2] = __private->gammaScale[3] = (1 << 16);
	__private->aliasMode         = kIODisplayModeIDInvalid;
	__private->offlineMode       = kIODisplayModeIDInvalid;
    DEBG(thisName, " invalidating aliasMode 0x%08x, currentDepth %d\n",
        __private->aliasMode, __private->currentDepth);

	if (!nowOnline) do
	{
		TIMESTART();
        FB_START(getDisplayModeCount,0,__LINE__,0);
        modeCount = getDisplayModeCount();
        FB_END(getDisplayModeCount,0,__LINE__,0);
		modeIDs = IONew(IODisplayModeID, modeCount);
		if (!modeIDs)
			break;
        FB_START(getDisplayModes,0,__LINE__,0);
		err = getDisplayModes(modeIDs);
        FB_END(getDisplayModes,err,__LINE__,0);
		if (kIOReturnSuccess == err)
		{
			for (modeNum = 0; modeNum < modeCount; modeNum++)
			{
				info.flags = 0;
                FB_START(getTimingInfoForDisplayMode,modeIDs[modeNum],__LINE__,0);
				err = getTimingInfoForDisplayMode(modeIDs[modeNum], &info);
                FB_END(getTimingInfoForDisplayMode,err,__LINE__,0);
				if (kIOReturnSuccess != err)
					continue;
				if (kIOTimingIDApple_0x0_0hz_Offline != info.appleTimingID)
					continue;
				__private->offlineMode = modeIDs[modeNum];
				break;
			}
		}
		IODelete(modeIDs, IODisplayModeID, modeCount);
		TIMEEND(thisName, "offline mode %x time: %qd ms\n", (int32_t) __private->offlineMode);
	}
	while (false);

    IOFB_END(updateOnline,nowOnline,0,0);
    return (nowOnline);
}

void IOFramebuffer::displaysOnline(bool nowOnline)
{
    IOFB_START(displaysOnline,nowOnline,0,0);
	if (nowOnline == __private->displaysOnline)
    {
        IOFB_END(displaysOnline,0,__LINE__,0);
		return;
    }

	if (nowOnline)
    {
		TIMESTART();
        IODisplayWrangler::makeDisplayConnects(this);
		TIMEEND(thisName, "makeDisplayConnects time: %qd ms\n");
	}
	else
	{
        OSSafeReleaseNULL(__private->displayPrefKey);
	
		TIMESTART();
		if (__private->paramHandler)
			__private->paramHandler->setDisplay(0);
		TIMEEND(thisName, "setDisplay time: %qd ms\n");

		TIMESTART();
		stopCursor();
//		temporaryPowerClampOn();
		//        FCUNLOCK(controller);
//		IODisplayWrangler::destroyDisplayConnects(this);
		//        FCLOCK(controller);
		TIMEEND(thisName, "destroyDisplayConnects time: %qd ms\n");
	}
	__private->displaysOnline = nowOnline;

	__private->actualVBLCount = 0;
    StdFBShmem_t * shmem = GetShmem(this);
	if (shmem)
	{
		shmem->vblDrift         = 0;
		shmem->vblDeltaMeasured = 0;
	}
	if (nowOnline)
	{
		__private->controller->fOnlineMask |=  (1 << __private->controllerIndex);
		updateVBL(this, NULL);
	}
	else	
	{
		__private->controller->fOnlineMask &= ~(1 << __private->controllerIndex);
	}
    IOFB_END(displaysOnline,0,0,0);
}

IOReturn IOFramebuffer::doSetDetailedTimings(OSArray *arr, uint64_t source, uint64_t line)
{
    IOReturn err;

    FB_START(setDetailedTimings,0,line,0);
    IOG_KTRACE_NT(DBG_IOG_SET_DETAILED_TIMING, DBG_FUNC_START,
                  source, __private->regID, 0, 0);
    err = setDetailedTimings(arr);
    IOG_KTRACE_NT(DBG_IOG_SET_DETAILED_TIMING, DBG_FUNC_END,
                  source, __private->regID, err, 0);
    FB_END(setDetailedTimings,err,line,0);

#if RLOG
    const int32_t count = arr ? arr->getCount() : -1;
    D(GENERAL, thisName, " set %d timings; status 0x%08x\n", count, err);
    for (int32_t i = 0; i < count; i++) {
        OSData *data = OSDynamicCast(OSData, arr->getObject(i));
        IODetailedTimingInformationV2 *timing =
            (IODetailedTimingInformationV2 *)data->getBytesNoCopy();
        D(DISPLAY_MODES, thisName, " 0x%08x %dx%d %dx%d\n",
            timing->detailedTimingModeID,
            timing->horizontalScaled,
            timing->verticalScaled,
            timing->horizontalActive,
            timing->verticalActive);
    }
#endif

    return err;
}

IOReturn IOFramebuffer::matchFramebuffer(void)
{
    IOFB_START(matchFramebuffer,0,0,0);
	IOFBDisplayModeDescription  modeInfo;
	IOReturn		err;
	IODisplayModeID mode;
	IOIndex         depth = 0;
	OSData *		data;
	OSArray *		array;
	
	modeInfo.timingInfo = __private->timingInfo;
	mode = kIODisplayModeIDInvalid;
	if (kIODetailedTimingValid & modeInfo.timingInfo.flags) do
	{
		modeInfo.timingInfo.detailedInfo.v2.detailedTimingModeID
				= (kIODisplayModeIDReservedBase | kIODisplayModeIDAliasBase);
		modeInfo.timingInfo.detailedInfo.v2.minPixelClock
				= modeInfo.timingInfo.detailedInfo.v2.pixelClock;
		modeInfo.timingInfo.detailedInfo.v2.maxPixelClock
				= modeInfo.timingInfo.detailedInfo.v2.pixelClock;

        FB_START(validateDetailedTiming,0,__LINE__,0);
		err = validateDetailedTiming(&modeInfo, sizeof(modeInfo));
        FB_END(validateDetailedTiming,err,__LINE__,0);
		if (kIOReturnSuccess != err)
		{
			DEBG1(thisName, " validateDetailedTiming(%x)\n", err);
			break;
		}	
		data = OSData::withBytes(&modeInfo.timingInfo.detailedInfo.v2,
										  sizeof(modeInfo.timingInfo.detailedInfo.v2));
		array = OSArray::withObjects((const OSObject**) &data, 1, 1);
		data->release();
		err = doSetDetailedTimings(array, DBG_IOG_SOURCE_MATCH_FRAMEBUFFER, __LINE__);
        array->release();
        if (kIOReturnSuccess != err)
        {
            break;
        }
		mode = kIODisplayModeIDReservedBase | kIODisplayModeIDAliasBase;
	}
	while (false);
	if (kIODisplayModeIDInvalid != mode)
	{
		setDisplayAttributes(__private->displayAttributes);

		depth = closestDepth(mode, &__private->pixelInfo);

        sendLimitState(__LINE__);

		TIMESTART();
        FB_START(setDisplayMode,0,__LINE__,0);
        IOG_KTRACE(DBG_IOG_SET_DISPLAY_MODE, DBG_FUNC_START,
                   0, DBG_IOG_SOURCE_MATCH_FRAMEBUFFER,
                   0, __private->regID,
                   0, mode,
                   0, depth);
        err = setDisplayMode(mode, depth);
        IOG_KTRACE(DBG_IOG_SET_DISPLAY_MODE, DBG_FUNC_END,
                   0, DBG_IOG_SOURCE_MATCH_FRAMEBUFFER,
                   0, __private->regID,
                   0, err, 0, 0);
        FB_END(setDisplayMode,err,__LINE__,0);
		TIMEEND(thisName, "matching setDisplayMode(0x%x, %d) err %x time: %qd ms\n",
				(int32_t) mode, (int32_t) depth, err);
        if (kIOReturnSuccess == err)
        {
            __private->lastSuccessfulMode = mode;
        }

		if (__private->rawGammaData)
		{
			TIMESTART();
            updateGammaTable(__private->rawGammaChannelCount,
                             __private->rawGammaDataCount,
                             __private->rawGammaDataWidth,
                             __private->rawGammaData,
                             kIOFBSetGammaSyncNotSpecified,
                             false, /*immediate*/
                             true /*ignoreTransactionActive*/);
			TIMEEND(thisName, "match updateGammaTable time: %qd ms\n");
		}
	}
    DEBG(thisName, " saving aliasMode 0x%08x, currentDepth 0x%08x\n", mode, depth);
	__private->aliasMode    = mode;
	__private->currentDepth = depth;
    IOFB_END(matchFramebuffer,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::setPowerState( unsigned long powerStateOrdinal,
                                       IOService * whichDevice )
{
    IOFB_START(setPowerState,powerStateOrdinal,0,0);
	IOReturn ret = kIOPMAckImplied;

    const uint64_t metricFunc
        = GMETRIC_DATA_FROM_MARKER(kGMETRICS_MARKER_EXITING_WAKE)
        | GMETRIC_DATA_FROM_FUNC(DBG_IOG_FB_POWER_CHANGE);
    const uint64_t metricDomain
        = (gIOFBPowerState == 2 && powerStateOrdinal < 2)
        ? (kGMETRICS_DOMAIN_FRAMEBUFFER | kGMETRICS_DOMAIN_POWER | kGMETRICS_DOMAIN_WAKE)
        : kGMETRICS_DOMAIN_NONE;
    GMETRIC(metricFunc, kGMETRICS_EVENT_SIGNAL, metricDomain);
    IOG_KTRACE(DBG_IOG_FB_POWER_CHANGE, DBG_FUNC_NONE,
               0, __private->regID,
               0, powerStateOrdinal, 0, 0, 0, 0);

    DEBG1(thisName, " (%ld) isMuted %d, now %d\n", powerStateOrdinal,
          __private->controller->isMuted(), (int) getPowerState());

	if (gIOFBSystemPowerAckTo && !powerStateOrdinal) {
        // Alias for readability
        const auto& sentPower  = __private->fServerMsgIDSentPower;
        const auto& ackedPower = __private->fServerMsgIDAckedPower;
		IOLog("graphics notify timeout (%x, %x)", ackedPower, sentPower);
    }

    FBGATEGUARD(ctrlgated, this);
    if (!__private->closed)
	{
		pendingPowerState = static_cast<unsigned int>(powerStateOrdinal);
		pendingPowerChange = true;

		__private->controller->startThread();
		ret = kPowerStateTimeout * 1000 * 1000;
	}

    IOFB_END(setPowerState,ret,0,0);
    return (ret);
}

IOReturn IOFramebuffer::powerStateWillChangeTo( IOPMPowerFlags flags,
        unsigned long state, IOService * whatDevice )
{
    IOFB_START(powerStateWillChangeTo,flags,state,0);
    DEBG1(thisName, " (%08lx)\n", flags);

	FBLOCK(this);
    if (isUsable && !(IOPMDeviceUsable & flags))
    {
		__private->pendingUsable = false;
		FBWL(this)->wakeupGate(&__private->pendingUsable, kManyThreadWakeup);
		FBUNLOCK(this);

		IOFramebuffer * fb;
        FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
		{
            FBGATEGUARD(ctrlgated, fb);
			if (fb->__private->display && fb->__private->pendingUsable)
			{
                AbsoluteTime deadline;
                clock_interval_to_deadline(1000, kMillisecondScale, &deadline );
				FBWL(fb)->sleepGate(&fb->__private->pendingUsable, deadline, THREAD_UNINT);
			}
		}

		FBLOCK(this);
		isUsable = false;
		__private->controller->didWork(kWorkStateChange);
	}
	FBUNLOCK(this);

    IOFB_END(powerStateWillChangeTo,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::powerStateDidChangeTo( IOPMPowerFlags flags,
        unsigned long, IOService* whatDevice )
{
    IOFB_START(powerStateDidChangeTo,flags,0,0);
    DEBG1(thisName, " (%08lx)\n", flags);

    FBGATEGUARD(ctrlgated, this);
    if ((IOPMDeviceUsable & flags) && !isUsable)
    {
		isUsable = __private->pendingUsable = true;
		__private->controller->didWork(kWorkStateChange);
	}

    IOFB_END(powerStateDidChangeTo,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}


void IOFramebuffer::updateDisplaysPowerState(void)
{
    IOFB_START(updateDisplaysPowerState,0,0,0);
    triggerEvent(kIOFBEventDisplaysPowerState);
    IOFB_END(updateDisplaysPowerState,0,0,0);
}

void IOFramebuffer::delayedEvent(thread_call_param_t p0, thread_call_param_t p1)
{
    IOFB_START(delayedEvent,0,0,0);
    const auto flags = reinterpret_cast<uintptr_t>(p1);
    triggerEvent(static_cast<unsigned>(flags));
    IOFB_END(delayedEvent,0,0,0);
}

void IOFramebuffer::resetClamshell(uint32_t delay, uint32_t where)
{
    IOFB_START(resetClamshell,delay,where,0);
	AbsoluteTime deadline;
	clock_interval_to_deadline(delay, kMillisecondScale, &deadline );
	thread_call_enter1_delayed(gIOFBClamshellCallout,
            (thread_call_param_t) kIOFBEventResetClamshell, deadline );
    IOG_KTRACE(DBG_IOG_CLAMSHELL, DBG_FUNC_NONE,
               0, DBG_IOG_SOURCE_RESET_CLAMSHELL, 0, where, 0, delay, 0, 0);
    IOFB_END(resetClamshell,0,where,0);
}

void IOFramebuffer::displayOnline(IODisplay * display, SInt32 delta, uint32_t options)
{
    IOFB_START(displayOnline,delta,options,0);
	if (delta <= 0)
	{
		options = __private->displayOptions;
	}
	if (kIODisplayOptionBacklight & options)
		OSAddAtomic(delta, &gIOFBBacklightDisplayCount);
	else
		OSAddAtomic(delta, &gIOFBDisplayCount);

    __private->fBuiltInPanel |= (0 != (kIODisplayOptionBacklight & options));

	if (delta < 0)
	{
//		if (display != __private->display) panic("(display != __private->display)");
		__private->display        = NULL;
		FBWL(this)->wakeupGate(&__private->pendingUsable, kManyThreadWakeup);
		__private->displayOptions = 0;
	}
	else
	{
//		if (__private->display) panic("(__private->display)");
		__private->display        = display;
		__private->displayOptions = options;
	}
    IOFB_END(displayOnline,0,0,0);
}

enum
{
    gMux_Message                = 'gMUX',
    gMux_WillSwitch             = 0,
    gMux_DidSwitch              = 1,
    gMux_DidNotSwitch           = 2,
};

#if DEBG_CATEGORIES_BUILD
static const char * const
muxSwitchStateStr(uintptr_t msg)
{
    switch (msg) {
#define CASE(x) case gMux_ ## x: return #x
        CASE(WillSwitch);
        CASE(DidSwitch);
        CASE(DidNotSwitch);
        default: return "Unknown";
#undef CASE
    }
}
#endif // DEBG_CATEGORIES_BUILD

/* static */
IOReturn IOFramebuffer::agcMessage( void * target, void * refCon,
        UInt32 messageType, IOService * service,
        void * messageArgument, vm_size_t argSize )
{
    IOFB_START(agcMessage,messageType,argSize,0);
    IOReturn ret = kIOReturnSuccess;

	IOService *       ackTo  = NULL;
	uint32_t          ackRef = 0;
	const uintptr_t   switchState = (uintptr_t) messageArgument;

    if (gMux_Message != messageType)
    {
        IOFB_END(agcMessage,ret,__LINE__,0);
		return (ret);
    }

	D(MUX, "MUX", " %s (%#lx)\n", muxSwitchStateStr(switchState), switchState);
    IOG_KTRACE(DBG_IOG_AGC_MSG, DBG_FUNC_NONE,
               0, switchState, 0, 0, 0, 0, 0, 0);

    {
        SYSGATEGUARD(sysgated);

        if (gMux_WillSwitch == switchState)
        {
            gIOFBIsMuxSwitching = true;
        }
        else if (gIOFBIsMuxSwitching && ((gMux_DidSwitch == switchState)
             || (gMux_DidNotSwitch == switchState)))
        {
            gIOFBIsMuxSwitching = false;

            ackTo  = gIOFBSystemPowerMuxAckTo;
            ackRef = gIOFBSystemPowerMuxAckRef;
            gIOFBSystemPowerMuxAckTo = 0;

            startThread(false);
        }
    }

	if (ackTo)
	{
        D(MUX, "MUX", " allowPowerChange\n");
        IOG_KTRACE(DBG_IOG_MUX_ALLOW_POWER_CHANGE, DBG_FUNC_NONE,
                   0, 0, 0, 0, 0, 0, 0, 0);
		ackTo->allowPowerChange(ackRef);
	}

    IOFB_END(agcMessage,ret,0,0);
    return (ret);
}


IOReturn IOFramebuffer::muxPowerMessage(UInt32 messageType)
{
    IOFB_START(muxPowerMessage,messageType,0,0);
    IOReturn ret = kIOReturnSuccess;

    DEBG1("PWR", " muxPowerMessage(%x->%x)\n",
          (int) gIOFBLastMuxMessage, (int) messageType);

    const bool isPowerOn = kIOMessageSystemWillPowerOn == messageType
                        || kIOMessageDeviceWillPowerOn == messageType;
    const bool isSleep   = kIOMessageSystemWillSleep   == messageType;
    uint64_t metricsDomain
        =  kGMETRICS_DOMAIN_FRAMEBUFFER | kGMETRICS_DOMAIN_POWER
        | (isPowerOn ? kGMETRICS_DOMAIN_WAKE : 0)
        | (isSleep ? (kGMETRICS_DOMAIN_SLEEP| kGMETRICS_DOMAIN_DOZE) : 0);
    (void) metricsDomain;
    GMETRICFUNC(DBG_IOG_MUX_POWER_MESSAGE, kGMETRICS_EVENT_START,
                metricsDomain);
    IOG_KTRACE(DBG_IOG_MUX_POWER_MESSAGE, DBG_FUNC_START,
               0, messageType, 0, 0, 0, 0, 0, 0);

	if (messageType == gIOFBLastMuxMessage)
    {
        GMETRICFUNC(DBG_IOG_MUX_POWER_MESSAGE, kGMETRICS_EVENT_END,
                    metricsDomain);
        IOG_KTRACE(DBG_IOG_MUX_POWER_MESSAGE, DBG_FUNC_END,
                   0, kIOReturnSuccess,
                   0, -1, 0, 0, 0, 0);

        IOFB_END(muxPowerMessage,kIOReturnSuccess,__LINE__,0);
		return (kIOReturnSuccess);
    }

	if (kIOMessageSystemWillPowerOn == messageType)
	{
        atomic_fetch_or(&gIOFBGlobalEvents, kIOFBEventSystemPowerOn);

        GMETRICFUNC(DBG_IOG_MUX_POWER_MESSAGE, kGMETRICS_EVENT_END,
                    metricsDomain);
        IOG_KTRACE(DBG_IOG_MUX_POWER_MESSAGE, DBG_FUNC_END,
                   0, kIOReturnSuccess,
                   0, -2, 0, 0, 0, 0);

        IOFB_END(muxPowerMessage,kIOReturnSuccess,__LINE__,0);
		return (kIOReturnSuccess);
	}
	if (kIOMessageDeviceWillPowerOn == messageType) {
        metricsDomain = kGMETRICS_DOMAIN_FRAMEBUFFER | kGMETRICS_DOMAIN_POWER
                      | kGMETRICS_DOMAIN_WAKE;
        messageType = kIOMessageSystemWillPowerOn;
    }

	if ((kIOMessageSystemHasPoweredOn == messageType) 
    &&  (kIOMessageSystemWillPowerOn != gIOFBLastMuxMessage))
	{
		DEBG1("PWR", " muxPowerMessage(%x)\n",
              (int) kIOMessageSystemWillPowerOn);
		if (gIOGraphicsControl) {
            (void) gIOGraphicsControl->
                message(kIOMessageSystemWillPowerOn, NULL, NULL);
        }
	}

	gIOFBLastMuxMessage = messageType;
	DEBG1("PWR", " muxPowerMessage(%x)\n", (int) messageType);
	if (gIOGraphicsControl)
        ret = gIOGraphicsControl->message(messageType, NULL, NULL);

    GMETRICFUNC(DBG_IOG_MUX_POWER_MESSAGE, kGMETRICS_EVENT_END, metricsDomain);
    IOG_KTRACE(DBG_IOG_MUX_POWER_MESSAGE, DBG_FUNC_END,
               0, ret, 0, 0, 0, 0, 0, 0);

    IOFB_END(muxPowerMessage,ret,0,0);
	return (ret);
}

IOReturn IOFramebuffer::systemPowerChange( void * target, void * refCon,
        UInt32 messageType, IOService * service,
        void * messageArgument, vm_size_t argSize )
{
    IOFB_START(systemPowerChange,messageType,argSize,0);
    IOReturn ret = kIOReturnSuccess;

    IOG_KTRACE_LOG_SYNCH(DBG_IOG_LOG_SYNCH);

    static const uint64_t kSystemPowerChangeDomain =
          kGMETRICS_DOMAIN_FRAMEBUFFER | kGMETRICS_DOMAIN_POWER
        | kGMETRICS_DOMAIN_WAKE        | kGMETRICS_DOMAIN_DOZE
        | kGMETRICS_DOMAIN_SLEEP       | kGMETRICS_DOMAIN_TERMINATION;
    // In the corresponding DBG_FUNC_END trace, we set the same broad set of
    // domains instead of more accurate ones so we don't get in a situation
    // where we record the START event but not the END one.
    GMETRICFUNC(DBG_IOG_SYSTEM_POWER_CHANGE, kGMETRICS_EVENT_START,
                kSystemPowerChangeDomain);
    IOG_KTRACE(DBG_IOG_SYSTEM_POWER_CHANGE, DBG_FUNC_START,
               0, messageType, 0, 0, 0, 0, 0, 0);

    DEBG1("PWR", "(%08x)\n", (uint32_t) messageType);

    // Let AGC know we have observed a system power change.
	if (gIOGraphicsControl) switch (messageType)
	{
#if VERSION_MAJOR >= 11
        case kIOMessageSystemCapabilityChange:
#endif
        case kIOMessageSystemWillSleep:
        case kIOMessageSystemWillPowerOn:
        case kIOMessageSystemHasPoweredOn:
			break;

		default:
			ret = gIOGraphicsControl->message(messageType, service, messageArgument);
			break;
	}

    switch (messageType)
    {
        case kIOMessageSystemCapabilityChange:
        {
            IOPMSystemCapabilityChangeParameters * params = (IOGRAPHICS_TYPEOF(params)) messageArgument;

            // root domain won't overlap capability changes with pstate changes

#define X(field, flag) \
    static_cast<bool>(params->field & kIOPMSystemCapability ## flag)
            const auto&    will = X(changeFlags,      WillChange);
            const auto&     did = X(changeFlags,      DidChange);
            const auto& fromGFX = X(fromCapabilities, Graphics);
            const auto&   toGFX = X(toCapabilities,   Graphics);
            const auto& fromCPU = X(fromCapabilities, CPU);
            const auto&   toCPU = X(toCapabilities,   CPU);
#undef X

            DEBG1("DARK", " %s%s 0x%x->0x%x\n",
                will ? "will" : "",
                did ? "did" : "",
                params->fromCapabilities,
                params->toCapabilities);


            if (will && fromGFX && !toGFX) {
                ret = muxPowerMessage(kIOMessageSystemWillSleep);
                if (kIOReturnNotReady == ret) {
                    SYSGATEGUARD(sysgated);
                    gIOFBIsMuxSwitching = true;
                    gIOFBSystemPowerMuxAckRef = params->notifyRef;
                    gIOFBSystemPowerMuxAckTo  = service;
                    params->maxWaitForReply = gIOGNotifyTO * 1000 * 1000;
                }
            } else if (will && !fromGFX && toGFX) {
                if (kIOMessageSystemHasPoweredOn != gIOFBLastMuxMessage) {
                    muxPowerMessage(kIOMessageSystemWillPowerOn);
                }
            } else if (did && !fromGFX && toGFX) {
                muxPowerMessage(kIOMessageSystemHasPoweredOn);
            } else if (did && !fromCPU && toCPU) {
                muxPowerMessage(kIOMessageSystemHasPoweredOn);
            } else if (will && !fromCPU && toCPU) {
                GMETRIC(
                    GMETRIC_DATA_FROM_MARKER(kGMETRICS_MARKER_EXITING_SLEEP)
                        | GMETRIC_DATA_FROM_FUNC(DBG_IOG_SYSTEM_POWER_CHANGE),
                    kGMETRICS_EVENT_SIGNAL,
                    kGMETRICS_DOMAIN_FRAMEBUFFER
                        | kGMETRICS_DOMAIN_POWER | kGMETRICS_DOMAIN_SLEEP);

                muxPowerMessage(kIOMessageSystemWillPowerOn);

                SYSGATEGUARD(sysgated);
                gIOFBSystemPower       = true;
                gIOGraphicsSystemPower = true;
                gIOFBSystemDark        = true;
            } else if (will && fromCPU && !toCPU) {
                if (gIOFBSystemPower) {
                    ret = muxPowerMessage(kIOMessageSystemWillSleep);

                    SYSGATEGUARD(sysgated);

                    gIOFBSystemPower       = false;
                    gIOFBSystemPowerAckRef = (void *)(uintptr_t) params->notifyRef;
                    gIOFBSystemPowerAckTo  = service;
                    startThread(false);
                    gIOGraphicsSystemPower = false;

                    // We will ack within gIOGNotifyTO seconds
                    params->maxWaitForReply = gIOGNotifyTO * 1000 * 1000;
                    ret                    = kIOReturnSuccess;
                }
            }
            ret = kIOReturnSuccess;

            GMETRICFUNC(DBG_IOG_SYSTEM_POWER_CHANGE, kGMETRICS_EVENT_END,
                        kSystemPowerChangeDomain);
            IOG_KTRACE(DBG_IOG_SYSTEM_POWER_CHANGE, DBG_FUNC_END,
                       0, messageType,
                       0, params->fromCapabilities,
                       0, params->toCapabilities,
                       0, params->changeFlags);
            break;
        }

        case kIOMessageSystemWillSleep:
		{
            GMETRIC(
                GMETRIC_DATA_FROM_MARKER(kGMETRICS_MARKER_ENTERING_SLEEP)
                    | GMETRIC_DATA_FROM_FUNC(DBG_IOG_SYSTEM_POWER_CHANGE),
                kGMETRICS_EVENT_SIGNAL,
                kGMETRICS_DOMAIN_FRAMEBUFFER
                    | kGMETRICS_DOMAIN_POWER | kGMETRICS_DOMAIN_SLEEP);
			IOPowerStateChangeNotification *params = (IOGRAPHICS_TYPEOF(params)) messageArgument;

			ret = muxPowerMessage(kIOMessageSystemWillSleep);

            SYSGATEGUARD(sysgated);

 			gIOFBClamshellState = kIOPMDisableClamshell;

            IOG_KTRACE(DBG_IOG_RECEIVE_POWER_NOTIFICATION, DBG_FUNC_NONE,
                       0, DBG_IOG_PWR_EVENT_SYSTEMPWRCHANGE,
                       0, kIOPMDisableClamshell, 0, 0, 0, 0);

			getPMRootDomain()->receivePowerNotification(kIOPMDisableClamshell);

			gIOFBSystemPower       = false;
			gIOFBSystemPowerAckRef = params->powerRef;
			gIOFBSystemPowerAckTo  = service;
			gIOFBIsMuxSwitching = (kIOReturnNotReady == ret);
			if (gIOFBIsMuxSwitching)
			{
				DEBG1("PWR", " agc not ready\n");
			}
			startThread(false);
			gIOGraphicsSystemPower = false;

			// We will ack within gIOGNotifyTO seconds
			params->returnValue    = gIOGNotifyTO * 1000 * 1000;
            ret                    = kIOReturnSuccess;

            GMETRICFUNC(DBG_IOG_SYSTEM_POWER_CHANGE, kGMETRICS_EVENT_END,
                        kSystemPowerChangeDomain);
            IOG_KTRACE(DBG_IOG_SYSTEM_POWER_CHANGE, DBG_FUNC_END,
                       0, messageType, 0, ret, 0, 0, 0, 0);
            break;
		}

        case kIOMessageSystemWillPowerOn:
		{
            GMETRIC(GMETRIC_DATA_FROM_MARKER(kGMETRICS_MARKER_ENTERING_WAKE)
                        | GMETRIC_DATA_FROM_FUNC(DBG_IOG_SYSTEM_POWER_CHANGE),
                    kGMETRICS_EVENT_SIGNAL,
                    kGMETRICS_DOMAIN_FRAMEBUFFER | kGMETRICS_DOMAIN_POWER
                        | kGMETRICS_DOMAIN_WAKE);

			IOPowerStateChangeNotification * params = (IOGRAPHICS_TYPEOF(params)) messageArgument;

			gIOFBSystemDark        = false;

			if (!gIOFBSystemPower)
			{
				muxPowerMessage(kIOMessageSystemWillPowerOn);

                SYSGATEGUARD(sysgated);
		
				readClamshellState(DBG_IOG_SOURCE_SYSWILLPOWERON);
                clearEvent(
                        kIOFBEventResetClamshell | kIOFBEventEnableClamshell);
				gIOFBSystemPower       = true;
				gIOGraphicsSystemPower = true;
			}
            params->returnValue    = 0;
            ret                    = kIOReturnSuccess;

            GMETRICFUNC(DBG_IOG_SYSTEM_POWER_CHANGE, kGMETRICS_EVENT_END,
                        kSystemPowerChangeDomain);
            IOG_KTRACE(DBG_IOG_SYSTEM_POWER_CHANGE, DBG_FUNC_END,
                       0, messageType, 0, ret, 0, 0, 0, 0);
            break;
		}

        case kIOMessageSystemHasPoweredOn:
		{
			IOPowerStateChangeNotification * params = (IOGRAPHICS_TYPEOF(params)) messageArgument;

			muxPowerMessage(kIOMessageSystemHasPoweredOn);

            SYSGATEGUARD(sysgated);
			startThread(false);
            params->returnValue = 0;
            ret                 = kIOReturnSuccess;

            GMETRICFUNC(DBG_IOG_SYSTEM_POWER_CHANGE, kGMETRICS_EVENT_END,
                        kSystemPowerChangeDomain);
            IOG_KTRACE(DBG_IOG_SYSTEM_POWER_CHANGE, DBG_FUNC_END,
                       0, messageType, 0, ret, 0, 0, 0, 0);
            break;
		}

        case kIOMessageSystemWillRestart:
        case kIOMessageSystemWillPowerOff:
        case kIOMessageSystemPagingOff:
		{
			IOPowerStateChangeNotification * params = (IOGRAPHICS_TYPEOF(params)) messageArgument;

            SYSGATEGUARD(sysgated);
            if (gAllFramebuffers)
            {
                IOFramebuffer * fb;
                FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
                {
                    FBGATEGUARD(ctrlgated, fb);

                    fb->deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );
					if (kIOMessageSystemPagingOff != messageType)
					{
                        FB_START(setAttribute,kIOSystemPowerAttribute,__LINE__,messageType);
						fb->setAttribute( kIOSystemPowerAttribute, messageType );
                        FB_END(setAttribute,0,__LINE__,0);
						fb->__private->closed = true;
					}
                }
				if (kIOMessageSystemPagingOff == messageType) saveGammaTables();
            }

            params->returnValue = 0;
            ret                 = kIOReturnSuccess;

            GMETRICFUNC(DBG_IOG_SYSTEM_POWER_CHANGE, kGMETRICS_EVENT_END,
                        kSystemPowerChangeDomain);
            IOG_KTRACE(DBG_IOG_SYSTEM_POWER_CHANGE, DBG_FUNC_END,
                       0, messageType, 0, ret, 0, 0, 0, 0);
            break;
		}

        default:
            ret = kIOReturnUnsupported;

            GMETRICFUNC(DBG_IOG_SYSTEM_POWER_CHANGE, kGMETRICS_EVENT_END,
                        kSystemPowerChangeDomain);
            IOG_KTRACE(DBG_IOG_SYSTEM_POWER_CHANGE, DBG_FUNC_END,
                       0, messageType, 0, ret, 0, 0, 0, 0);
            break;
    }

    IOFB_END(systemPowerChange,ret,0,0);
    return (ret);
}

void IOFramebuffer::deferredSpeedChangeEvent( OSObject * owner,
                                              IOInterruptEventSource * evtSrc, int intCount )
{
    IOFB_START(deferredSpeedChangeEvent,intCount,0,0);
    IOFramebuffer *fb = (IOFramebuffer *) owner;

    if (fb->__private->pendingSpeedChange)
    {
        thread_t saveThread = fb->__private->controller->setPowerThread();
        fb->__private->pendingSpeedChange = false;
        FB_START(setAttribute,kIOFBSpeedAttribute,__LINE__,fb->__private->reducedSpeed);
        fb->setAttribute(kIOFBSpeedAttribute, fb->__private->reducedSpeed);
        FB_END(setAttribute,0,__LINE__,0);
        fb->__private->controller->setPowerThread(saveThread);
    }
    IOFB_END(deferredSpeedChangeEvent,0,0,0);
}

IOReturn IOFramebuffer::setAggressiveness( unsigned long type, unsigned long newLevel )
{
    IOFB_START(setAggressiveness,type,newLevel,0);
    UInt32 reducedSpeed = static_cast<UInt32>(newLevel);

    if (__private
     && (type == (unsigned long) kIOFBLowPowerAggressiveness)
//     && (reducedSpeed != __private->reducedSpeed)
	)
    {
        __private->reducedSpeed       = reducedSpeed;
        __private->pendingSpeedChange = true;
        if (__private->allowSpeedChanges && __private->deferredSpeedChangeEvent)
		{
            FBGATEGUARD(ctrlgated, this);
			__private->controller->startThread();
//            __private->deferredSpeedChangeEvent->interruptOccurred(0, 0, 0);
		}
    }

    super::setAggressiveness(type, newLevel);

    IOFB_END(setAggressiveness,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

IOReturn 
IOFramebuffer::getAggressiveness( unsigned long type, unsigned long * currentLevel )
{
    IOFB_START(getAggressiveness,type,0,0);
    IOReturn ret;

    if (gIOFBSystemWorkLoop && (type == (unsigned long) kIOFBLowPowerAggressiveness))
    {
        SYSGATEGUARD(sysgated);

        *currentLevel = __private->reducedSpeed;
        ret = kIOReturnSuccess;
    }
    else
        ret = super::getAggressiveness(type, currentLevel);

    IOFB_END(getAggressiveness,ret,0,0);
    return (ret);
}

void
IOFramebuffer::serverAcknowledgeNotification(integer_t msgh_id)
{
    IOFB_START(serverAcknowledgeNotification,msgh_id,0,0);

    const bool& isModernServer = __private->fServerUsesModernAcks;
    if (!isModernServer && !msgh_id)
        msgh_id = __private->fServerMsgIDSentPower; // Emulate reflection ack

    // TODO(gvdl) GTrace rendezvous, not acked I think, ask ericd
    // const bool isRendezvous = msgh_id == kIOFBNS_Rendezvous;

    uint64_t sentAckedPower = 0;
    uint64_t hidden = 0;

    // Convenience aliases and instrumentation support
    auto& aliasAckedPower      = __private->fServerMsgIDAckedPower;
    const auto& aliasSentPower = __private->fServerMsgIDSentPower;
    sentAckedPower = GPACKUINT32T(1, aliasSentPower)
                   | GPACKUINT32T(0, aliasAckedPower);


    const auto messageType = msgh_id & kIOFBNS_MessageMask;

    const bool isPower = !isModernServer
        || (kIOFBNS_Sleep <= messageType && messageType <= kIOFBNS_Doze);

    uint64_t metricsDomain = 0;
    if (isPower) {
        const bool isWake  = kIOFBNS_Wake  == messageType;
        const bool isSleep = kIOFBNS_Sleep == messageType;
        const bool isDoze  = kIOFBNS_Doze  == messageType;
        metricsDomain = kGMETRICS_DOMAIN_FRAMEBUFFER | kGMETRICS_DOMAIN_POWER
                      | (isSleep ? kGMETRICS_DOMAIN_SLEEP : 0)
                      | (isWake  ? kGMETRICS_DOMAIN_WAKE  : 0)
                      | (isDoze  ? kGMETRICS_DOMAIN_DOZE  : 0);

        // Note server acknowledgement
        aliasAckedPower = msgh_id;
        if (isWake && __private->cursorSlept) {
            resetCursor();
            __private->cursorSlept = false;
        } else if (!isWake && !__private->cursorSlept) {
            hideCursor();
            __private->cursorSlept = true;
        }

        __private->fDeferDozeUntilAck = false;
        __private->controller->didWork(kWorkStateChange);
    }



    GMETRICFUNC(DBG_IOG_SERVER_ACK, kGMETRICS_EVENT_SIGNAL, metricsDomain);
    IOG_KTRACE_NT(DBG_IOG_SERVER_ACK, DBG_FUNC_NONE,
                  __private->regID, msgh_id, sentAckedPower, hidden);
    IOFB_END(serverAcknowledgeNotification,msgh_id,0,0);
}

IOReturn IOFramebuffer::
extAcknowledgeNotificationImpl(IOExternalMethodArguments * args)
{
    const bool& hasModernAcks = __private->fServerUsesModernAcks;
    const int maxInputs = 2 * kIOPreviewImageCount + hasModernAcks;
    // Round to even number if server isn't using modern acks or we ignore them
    const int numInputs = args->scalarInputCount & ~(!hasModernAcks);
    if (numInputs > maxInputs) {
        IOLog("IOFB::extAcknowledgeNotificationImpl %s"
              " Bad scalar argument list %d\n", thisName, numInputs);
        DEBG1(thisName, " Bad scalar argument list %d\n", numInputs);
        return kIOReturnBadArgument;
    }

    auto* const scalars = &args->scalarInput[0];
    const int numImages = (numInputs - hasModernAcks) / 2;

	for (int addrIdx = 0; addrIdx < kIOPreviewImageCount; addrIdx++) {
		const mach_vm_address_t address = scalars[addrIdx];
		const mach_vm_size_t   length   = scalars[numImages + addrIdx];

        OSSafeReleaseNULL(__private->saveBitsMD[addrIdx]);
		if (addrIdx < numImages && address && length) {
			__private->saveBitsMD[addrIdx]
                = IOMemoryDescriptor::withAddressRange(
                        // TODO(gvdl), why kIOMemoryPersistent not prepare()?
                        address, length, kIODirectionOut | kIOMemoryPersistent,
                        current_task());

            // Log using aliases for readability
            const auto& sentPower  = __private->fServerMsgIDSentPower;
            const auto& ackedPower = __private->fServerMsgIDAckedPower;
            (void) sentPower, (void) ackedPower; // 'Use' variables
			DEBG1(thisName,
                  " (%x->%x) save bits [0x%llx, 0x%llx] md %p\n",
				  ackedPower, sentPower, address, length,
				  __private->saveBitsMD[addrIdx]);
		}
	}

    // Grab reflected msgh_id when provided by WS/CD
    const int ackMsghID = (__private->fServerUsesModernAcks)
                        ? static_cast<int>(scalars[numInputs - 1]) : 0;
	serverAcknowledgeNotification(ackMsghID);

    return kIOReturnSuccess;
}

IOReturn IOFramebuffer::extAcknowledgeNotification(
            OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extAcknowledgeNotification,0,0,0);
    IOFramebuffer * inst = (IOFramebuffer *) target;
    uintptr_t errLine = 0;

    IOReturn err
        = inst->extEntry(true, kIOGReportAPIState_AcknowledgeNotification);
    if (err)
        errLine = __LINE__;
    else {
        err = inst->extAcknowledgeNotificationImpl(args);
        if (err)
            errLine = __LINE__;
    }

    inst->extExit(err, kIOGReportAPIState_AcknowledgeNotification);

    IOFB_END(extAcknowledgeNotification,err,errLine,0);
    return (err);
}

IOReturn IOFramebuffer::extRegisterNotificationPort(
    mach_port_t         port,
    UInt32              type,
    UInt32              refCon )
{
    IOFB_START(extRegisterNotificationPort,port,type,0);
    IOReturn            err;

    // Must hold system loop to issue a notifyServer
    if ((err = extEntrySys(/* allowOffline */ true,
                           kIOGReportAPIState_SetNotificationPort)))
    {
        IOFB_END(extRegisterNotificationPort,err,__LINE__,0);
        return (err);
    }

    __private->fServerUsesModernAcks =
        (type >= kServerAckProtocolGraphicsTypesRev);


    mach_msg_header_t *msgh = static_cast<mach_msg_header_t*>(serverMsg);
    bzero(msgh, sizeof(*msgh));

    msgh->msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    msgh->msgh_size        = sizeof(mach_msg_header_t);
    msgh->msgh_remote_port = port;

    // Can't use notifyServer, kIOFBNS_Rendezvous is 32 bit not uint8_t
    msgh->msgh_id = kIOFBNS_Rendezvous;
    (void) mach_msg_send_from_kernel(msgh, msgh->msgh_size);
    // Now send the negated version of IOGraphicsFamily
    msgh->msgh_id = -IOGRAPHICSTYPES_REV;
    (void) mach_msg_send_from_kernel(msgh, msgh->msgh_size);

    // server assumes so at startup
    uint8_t currentState = __private->fServerMsgIDSentPower;
    __private->fServerMsgIDSentPower = kIOFBNS_Wake;
    notifyServer(currentState);

    extExitSys(err, kIOGReportAPIState_SetNotificationPort);

    IOFB_END(extRegisterNotificationPort,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if IOFB_DISABLEFB
IODeviceMemory * IOFramebuffer::_getApertureRange(IOFramebuffer * fb, IOPixelAperture aperture)
{
    IOFB_START(_getApertureRange,aperture,0,0);
	typedef IODeviceMemory * (*Proc)(IOFramebuffer * fb, IOPixelAperture aperture);
	Proc proc = (Proc) fb->__private->controller->fSaveGAR;
    IODeviceMemory * mem;

	mem = (*proc)(fb, aperture);
//	kprintf("%s: zorch GAR(%d) %p\n", fb->thisName, aperture, mem);
	if (kIOFBSystemAperture == aperture) mem = 0;

    IOFB_END(_getApertureRange,0,0,0);
	return (mem);
}
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOFramebuffer::writePrefs( OSObject * null, IOTimerEventSource * sender )
{
    IOFB_START(writePrefs,0,0,0);
    IOFramebuffer * fb = (IOFramebuffer *) gAllFramebuffers->getObject(0);
	if (fb)
	{
		DEBG(fb->thisName, "\n");
		fb->messageClients( kIOMessageServicePropertyChange, (void *) 'pref' );
	}
    IOFB_END(writePrefs,0,0,0);
}

void IOFramebuffer::connectChangeInterruptImpl(uint16_t source)
{
    OSIncrementAtomic(&__private->controller->fConnectChange);
    const auto a1 =
        GPACKUINT32T(1, __private->lastProcessedChange) |
        GPACKUINT16T(0, source);
    const auto a2 =
        GPACKUINT32T(1, __private->controller->fLastFinishedChange) |
        GPACKUINT32T(0, __private->controller->fLastMessagedChange);
    const auto a3 =
        GPACKUINT32T(1, __private->controller->fLastForceRetrain) |
        GPACKUINT32T(0, __private->controller->fConnectChange);
    IOG_KTRACE_NT(DBG_IOG_CONNECT_CHANGE_INTERRUPT_V2, DBG_FUNC_NONE,
        __private->regID, a1, a2, a3);
    DEBG1(thisName, "(%d)\n", __private->controller->fConnectChange);
    startThread(false);
}

void IOFramebuffer::connectChangeInterrupt( IOFramebuffer * inst, void * )
{
    inst->connectChangeInterruptImpl(DBG_IOG_SOURCE_VENDOR);
}

/*! By design, GL indices identify bits in a 32-bit mask. */
#define kGLIndexMax 32

/*!
 * Find contiguous bits for all the FB's on this controller and assign one to
 * each FB. If a contiguous block isn't available, driver still opens, but
 * without setting IOFramebufferOpenGLIndex.
 *
 * Note since GL indices have to be contiguous for all FBs for a controller,
 * this is called once on the controller's first FB to open, and it assigns
 * indices for this FB and all its dependents.
 */
void IOFramebuffer::assignGLIndex(void)
{
    IOFB_START(assignGLIndex,0,0,0);
    SYSASSERTGATED();

    uint32_t numFbs = 0;
    IOFramebuffer *next = this;
    do {
        numFbs++;
    } while ((next = next->getNextDependent()) && (next != this));

    if (numFbs > kGLIndexMax) {
        IOLog("%u > kGLIndexMax\n", numFbs);
        IOFB_END(assignGLIndex,-1,__LINE__,0);
        return;
    }

    uint32_t glMask = (uint32_t)((1ULL << numFbs) - 1);
    uint32_t glShiftLimit = kGLIndexMax - numFbs;
    uint32_t glShift;

    for (glShift = 0; glShift <= glShiftLimit; glShift++) {
        if (0 == (gIOFBOpenGLMask & (glMask << glShift))) {
            break;
        }
    }

    if (glShift <= glShiftLimit) {
        DEBG1(thisName, " openGLIndex %u assigned 0x%08x 0x%08x\n",
            numFbs, gIOFBOpenGLMask, (glMask << glShift));
        gIOFBOpenGLMask |= (glMask << glShift);
        IOFramebuffer *tnext = this;
        uint32_t i = 0;
        do {
            tnext->__private->openGLIndex = (1 << (glShift + i));
            tnext->setProperty(kIOFramebufferOpenGLIndexKey, glShift + i, 64);
            i++;
        } while ((tnext = tnext->getNextDependent()) && (this != tnext));
    } else {
        DEBG1(thisName, " openGLIndex %u fail 0x%08x %d\n",
            numFbs, gIOFBOpenGLMask, numFbs);
    }
    IOFB_END(assignGLIndex,0,0,0);
}

IOReturn IOFramebuffer::open( void )
{
    IOFB_START(open,0,0,0);
    IOReturn            err = kIOReturnSuccess;
    uintptr_t           value;
    IOFramebuffer *     next;
    OSNumber *          depIDProp;
    OSNumber *          depIDMatch;
    OSNumber *          num;
    OSData *            data;
    OSObject *          obj;
    bool                newController;
    unsigned            i;

    if (!gIOFBSystemWorkLoop) panic("gIOFBSystemWorkLoop");
    SYSGATEGUARD(sysgated);

    do
    {
        if (opened) continue;
        if (dead)
        {
            err = kIOReturnNotOpen;
            continue;
        }

        if (!gIOFBServerInit) IOFramebuffer::initialize();

        if (!gAllFramebuffers
         || !gIOFBRootNotifier
         || !gIOFBHIDWorkLoop
         || !gIOFBWorkES
         || !IODisplayWrangler::serverStart())
        {
            err = kIOReturnVMError;
            continue;
        }

        if (!gIOFBClamshellNotify)
            gIOFBClamshellNotify = addMatchingNotification( gIOPublishNotification,
                                                       resourceMatching(kAppleClamshellStateKey),
                                                       &clamshellHandler, NULL, 0, 10000 );
		readClamshellState(DBG_IOG_SOURCE_OPEN);

        if (-1 == gIOFBHaveBacklight) do
        {
            OSDictionary * matching      = nameMatching("backlight");
            OSIterator *   iter          = NULL;
            bool           haveBacklight = false;
            if (matching)
            {
                iter = getMatchingServices(matching);
                matching->release();
            }
            if (iter)
            {
                haveBacklight = (0 != iter->getNextObject());
                iter->release();
            }
            gIOFBHaveBacklight = haveBacklight;

            if (!haveBacklight)
                continue;

            const OSSymbol * settingsArray[2];
            getPMRootDomain()->publishFeature("DisplayDims");   
            // Register to manage the "DisplaySleepUsesDim" setting
            settingsArray[0] = gIOFBPMSettingDisplaySleepUsesDimKey;
            settingsArray[1] = NULL;
            getPMRootDomain()->registerPMSettingController(settingsArray,
                                                            &pmSettingsChange,
                                                            NULL,
                                                            (uintptr_t) NULL,
                                                            &__private->pmSettingNotificationHandle);
        }
        while (false);

        __private->fServerMsgCount  = 0;
        __private->fServerMsgIDSentPower  = kIOFBNS_Wake;
        __private->fServerMsgIDAckedPower = kIOFBNS_Wake;
        
        if (!__private->controller)
        {
            __private->controller = copyController(IOFBController::kForceCreate);
            if (!__private->controller)
                panic("IOFBController");
        }
        newController = __private->controller->fState & kIOFBNotOpened;
        if (newController) {
            __private->controller->clearState(kIOFBNotOpened);
            __private->controller->fDidWork = kWorkStateChange;
        }
		if (__private->controller->fIntegrated) setProperty(kIOFBIntegratedKey, kOSBooleanTrue);

        FBGATEGUARD(ctrlgated, this);  // Take the controller lock

        probeAccelerator();

        __private->wsaaState = kIOWSAA_DriverOpen;
        __private->gammaSyncType = kIOFBSetGammaSyncNotSpecified;
        resetLimitState();

		uint32_t depIdx;
        depIDProp = OSDynamicCast(OSNumber, getProperty(kIOFBDependentIDKey));
        bool openAllDependents = depIDProp && !nextDependent;
        num = OSDynamicCast(OSNumber, getProperty(kIOFBDependentIndexKey));
		depIdx = num ? num->unsigned32BitValue() : 0;
		if (depIdx >= kIOFBControllerMaxFBs)
			panic("%s: bad " kIOFBDependentIndexKey "\n", getName());
		else
		{
			__private->controller->fFbs[depIdx] = this;
			if (depIdx >= __private->controller->fMaxFB)
				__private->controller->fMaxFB = depIdx;
			__private->controllerIndex = depIdx;
		}

		__private->controller->fWsWait |= (1 << __private->controllerIndex);
		isUsable = true;

		thisNameLen = (uint32_t)strlen(__private->controller->fName) + 3;
		char * logName = IONew(char, thisNameLen);
		if (logName) {
			snprintf(logName, thisNameLen, "%s-%c", __private->controller->fName, 'A' + depIdx);
            thisName = logName;
        } else {
            thisNameLen = 0;
        }

        // Link all of the framebuffers for this controller together
        if (openAllDependents)
        {
            IOFramebuffer *first = 0;
            IOFramebuffer *last = 0;
            for (i = 0;
                (next = (IOFramebuffer *)gStartedFramebuffers->getObject(i));
                i++)
            {
                depIDMatch = OSDynamicCast(OSNumber,
                                           next->getProperty(kIOFBDependentIDKey));
                if (!depIDMatch || !depIDMatch->isEqualTo(depIDProp))
                    continue;
                if (next->__private->controller)
                {
                    DEBG(next->thisName,
                         " %llx prelinked controller %p\n", next->__private->regID,
                         OBFUSCATE(__private->controller));
                    assert(next->__private->controller == __private->controller);
                }
                else
                {
                    DEBG(next->thisName,
                         " %llx linking controller %p\n", next->__private->regID,
                         OBFUSCATE(__private->controller));
                    next->__private->controller
                        = copyController(IOFBController::kLookupOnly);
                }
                if (!first)
                    first = next;
                else if (last)
                    last->setNextDependent(next);
                last = next;
            }
            if (first && last && (first != last))
                last->setNextDependent(first);
        }

        // tell the console if it's on this display, it's going away
        if (isConsoleDevice() /*&& !gIOFBConsoleFramebuffer*/)
            gIOFBConsoleFramebuffer = this;
        if ((this == gIOFBConsoleFramebuffer) || !gIOFBConsoleFramebuffer)
            setPlatformConsole(0, kPEDisableScreen, DBG_IOG_SOURCE_OPEN);

        // Console is off from here until the controller is up: on a machine
        // with no serial port this stretch is invisible, so say where we are.
        kprintf("IOFB: console disabled, notifying DisplayModeWillChange\n");
        deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );
        kprintf("IOFB: notified, calling enableController\n");

        IOService * provider = getProvider();
        provider->setProperty("AAPL,gray-value", gIOFBGray32Data);
        provider->setProperty("AAPL,gray-page", gIOFBOne32Data);
        data = OSData::withBytesNoCopy(&__private->uiScale, sizeof(__private->uiScale));
		if (data)
		{
			setProperty(kIOFBUIScaleKey, data);
			data->release();
		}

		if (!AbsoluteTime_to_scalar(&__private->controller->fInitTime))
			AbsoluteTime_to_scalar(&__private->controller->fInitTime) = mach_absolute_time();

        FB_START(enableController,0,__LINE__,0);
        err = enableController();
        FB_END(enableController,err,__LINE__,0);
        kprintf("IOFB: enableController returned 0x%x\n", err);
        if (kIOReturnSuccess != err)  // Vendor controller had a problem
        {
            dead = true;
            if (nextDependent)
            {
                nextDependent->setNextDependent( NULL );
                nextDependent = NULL;
            }
            deliverDisplayModeDidChangeNotification();
            continue;
        }

        if (newController)
		{
#if IOFB_DISABLEFB
			// getApertureRange 2472
			// getVRAMRange 2480
			uintptr_t * vt;
			addr64_t    v;
			ppnum_t     phys;
			vt = ((uintptr_t **)this)[0];
			__private->controller->fSaveGAR = vt[2472/sizeof(uintptr_t)];
			v = (addr64_t) &vt[2472/sizeof(uintptr_t)];
			phys = pmap_find_phys(kernel_pmap, v);
			ml_phys_write_double_64(ptoa_64(phys) | (v & page_mask), (uint64_t) &IOFramebuffer::_getApertureRange);
#endif

            assignGLIndex();
        }

        if (!__private->controller->fAliasID)
        {
            if ((obj = copyProperty("AAPL,display-alias", gIOServicePlane)))
            {
                if ((data = OSDynamicCast(OSData, obj))
                    &&   data->getLength() == sizeof(uint32_t))
                {
                    uint32_t word
                    = *(static_cast<const uint32_t*>(data->getBytesNoCopy()));
                    __private->controller->fAliasID = 0x80000000 | word;
                }
                else
                {
                    IOLog("%s: Malformed AAPL,display-alias property\n", thisName);
                }
                obj->release();
            }
        }

        DEBG(thisName, " this %p \"%s\" \"%s\"\n", OBFUSCATE(this), getName(), getProvider()->getName());
        DEBG1(thisName, " singleth %d, this %p controller %p\n", 
              SINGLE_THREAD, OBFUSCATE(this), OBFUSCATE(__private->controller));
        DEBG1(thisName, " init time now %lld start %lld\n", 
              mach_absolute_time(), AbsoluteTime_to_scalar(&__private->controller->fInitTime));
        
        pagingState = true;

		obj = copyProperty("graphic-options", gIOServicePlane);
		if (obj)
		{
			data = OSDynamicCast(OSData, obj);
			uint32_t gOpts = ((UInt32 *) data->getBytesNoCopy())[0];
			__private->colorModesAllowed = (data && (0 != (kIOGPlatformYCbCr & gOpts)));
			obj->release();
		}
		// Any XG is allowed to support YCbCr; no EFI, so no graphic-options property.
		__private->colorModesAllowed |= __private->controller->fExternal;

        if ((num = OSDynamicCast(OSNumber, getProperty(kIOFBTransformKey))))
            __private->selectedTransform = num->unsigned64BitValue();
        __private->selectedTransform |= kIOFBDefaultScalerUnderscan;

        // vbl events
        FB_START(registerForInterruptType,kIOFBVBLInterruptType,__LINE__,0);
        err = registerForInterruptType( kIOFBVBLInterruptType,
                                        (IOFBInterruptProc) &handleVBL,
                                        this, priv, &__private->vblInterrupt );
        FB_END(registerForInterruptType,err,__LINE__,0);
        haveVBLService = (err == kIOReturnSuccess );
        if (haveVBLService)
        {
            __private->deferredVBLDisableEvent = IOInterruptEventSource::interruptEventSource(
															this, &deferredVBLDisable);
			if (__private->deferredVBLDisableEvent)
				getWorkLoop()->addEventSource(__private->deferredVBLDisableEvent);
			__private->vblUpdateTimer = IOTimerEventSource::timerEventSource(this, &updateVBL);
			if (__private->vblUpdateTimer)
				getWorkLoop()->addEventSource(__private->vblUpdateTimer);
		}

        if (haveVBLService)
        {
            FB_START(getAttribute,kIODeferCLUTSetAttribute,__LINE__,0);
            IOReturn er = getAttribute( kIODeferCLUTSetAttribute, &value );
            FB_END(getAttribute,er,__LINE__,0);

            if ((kIOReturnSuccess == er) && value)
            {
                __private->deferredCLUTSetEvent = IOInterruptEventSource::interruptEventSource(
                                                                                               this, &deferredCLUTSetInterrupt);
                if (__private->deferredCLUTSetEvent)
                    getWorkLoop()->addEventSource(__private->deferredCLUTSetEvent);

                __private->deferredCLUTSetTimerEvent = IOTimerEventSource::timerEventSource(
                                                                                            this, &deferredCLUTSetTimer);
                if (__private->deferredCLUTSetTimerEvent)
                    getWorkLoop()->addEventSource(__private->deferredCLUTSetTimerEvent);
                
                if (__private->deferredCLUTSetEvent || __private->deferredCLUTSetTimerEvent)
                    setProperty(kIOFBCLUTDeferKey, kOSBooleanTrue);
            }
        }

        // connect events
		obj = copyProperty(kIOFBConnectInterruptDelayKey, gIOServicePlane);
        if (obj)
        {
            OSData * tdata;
            if ((tdata = OSDynamicCast(OSData, obj)))
                __private->delayedConnectTime = *((UInt32 *) tdata->getBytesNoCopy());
            obj->release();
        }

        FB_START(registerForInterruptType,kIOFBConnectInterruptType,__LINE__,0);
        err = registerForInterruptType( kIOFBConnectInterruptType,
                                        (IOFBInterruptProc) &connectChangeInterrupt,
                                        this, (void *)(uintptr_t) __private->delayedConnectTime,
                                        &__private->connectInterrupt );
        FB_END(registerForInterruptType,err,__LINE__,0);
        // dp events
        __private->dpInterrupDelayTime = 2;
        __private->dpInterruptES = IOTimerEventSource::timerEventSource(this, &dpInterrupt);
        if (__private->dpInterruptES)
            getWorkLoop()->addEventSource(__private->dpInterruptES);
    
        FB_START(registerForInterruptType,kIOFBDisplayPortInterruptType,__LINE__,0);
        err = registerForInterruptType( kIOFBDisplayPortInterruptType,
                                       (IOFBInterruptProc) &dpInterruptProc,
                                       this, (void *)(uintptr_t) __private->dpInterrupDelayTime,
                                       &__private->dpInterruptRef );
        FB_END(registerForInterruptType,err,__LINE__,0);
        __private->dpInterrupts = (kIOReturnSuccess == err);
        __private->dpSupported  = __private->dpInterrupts;
        //


        FB_START(getAttribute,kIOHardwareCursorAttribute,__LINE__,0);
        err = getAttribute( kIOHardwareCursorAttribute, &value );
        FB_END(getAttribute,err,__LINE__,0);
        haveHWCursor = ((err == kIOReturnSuccess) && (0 != (kIOFBHWCursorSupported & value)));

        clock_interval_to_deadline(40, kMicrosecondScale, 
                                    &__private->defaultI2CTiming.bitTimeout);
        clock_interval_to_deadline(40, kMicrosecondScale, 
                                    &__private->defaultI2CTiming.byteTimeout);
        clock_interval_to_deadline(40, kMicrosecondScale, 
                                    &__private->defaultI2CTiming.acknowledgeTimeout);
        clock_interval_to_deadline(40, kMicrosecondScale, 
                                    &__private->defaultI2CTiming.startTimeout);

        FB_START(getAttributeForConnection,kConnectionSupportsLLDDCSense,__LINE__,0);
        __private->lli2c = (kIOReturnSuccess == getAttributeForConnection(
                                        0, kConnectionSupportsLLDDCSense, 
                                        (uintptr_t *) &__private->defaultI2CTiming));
        FB_END(getAttributeForConnection,0,__LINE__,0);

        if ((num = OSDynamicCast(OSNumber, getProperty(kIOFBGammaWidthKey))))
            __private->desiredGammaDataWidth = num->unsigned32BitValue();
        if ((num = OSDynamicCast(OSNumber, getProperty(kIOFBGammaCountKey))))
            __private->desiredGammaDataCount = num->unsigned32BitValue();
        if ((num = OSDynamicCast(OSNumber, getProperty(kIOFBGammaHeaderSizeKey))))
            __private->gammaHeaderSize = num->unsigned32BitValue();

        __private->deferredSpeedChangeEvent = IOInterruptEventSource::interruptEventSource(
                                                                    this, deferredSpeedChangeEvent);
        if (__private->deferredSpeedChangeEvent)
            getWorkLoop()->addEventSource(__private->deferredSpeedChangeEvent);
        
        opened = true;

        bool nowOnline;
        nowOnline = updateOnline();
		if (nowOnline)
		{
			if (!gIOFBConsoleFramebuffer)        gIOFBConsoleFramebuffer = this;
		}
		else
		{
			if (this == gIOFBConsoleFramebuffer) gIOFBConsoleFramebuffer = NULL;
		}

        __private->paramHandler = IOFramebufferParameterHandler::withFramebuffer(this);
        if (__private->paramHandler)
            setProperty(gIODisplayParametersKey, __private->paramHandler);

        IOFramebufferI2CInterface::create(this, thisName);

        __private->online = nowOnline;
        if (nowOnline)
        	displaysOnline(nowOnline);
        else
            IODisplayUpdateNVRAM(this, 0);

        __private->transform = __private->selectedTransform;
        setProperty(kIOFBTransformKey, __private->transform, 64);

        uint32_t ii = gStartedFramebuffers->getNextIndexOfObject(this, 0);
        if ((unsigned) -1 != ii) gStartedFramebuffers->removeObject(ii);
        gAllFramebuffers->setObject(this);

        if (openAllDependents)
        {
            next = this;
            while ((next = next->getNextDependent()) && (next != this))
            {
                next->open();
            }
        }

        if (nowOnline)
        {
            deliverDisplayModeDidChangeNotification();
            err = kIOReturnSuccess;
        }
        else
        {
        	findConsole();
            deliverDisplayModeDidChangeNotification();
            dpUpdateConnect();
        }

        if (openAllDependents)
        {
            next = this;
            do
            {
                next->postOpen();
            }
            while ((next = next->getNextDependent()) && (next != this));
        }

        DEBG(thisName, " opened %d, started %d, gl 0x%08x\n",
            gAllFramebuffers->getCount(), gStartedFramebuffers->getCount(),
            gIOFBOpenGLMask);
    }
    while (false);

	if (__private->controller)
	{
        FBGATEGUARD(ctrlgated, this);

		__private->allowSpeedChanges = true;
		if (__private->pendingSpeedChange)
		{
            thread_t saveThread = __private->controller->setPowerThread();
			__private->pendingSpeedChange = false;
            FB_START(setAttribute,kIOFBSpeedAttribute,__LINE__,__private->reducedSpeed);
			setAttribute(kIOFBSpeedAttribute, __private->reducedSpeed);
            FB_END(setAttribute,0,__LINE__,0);
            __private->controller->setPowerThread(saveThread);
		}
	}

    DEBG(thisName, " %d\n", err);

    IOFB_END(open,err,0,0);
    return (err);
}

void IOFramebuffer::setTransform( UInt64 newTransform, bool generateChange )
{
    IOFB_START(setTransform,newTransform,generateChange,0);
    newTransform |= (kIOFBScalerUnderscan & __private->selectedTransform);

    if (newTransform != __private->selectedTransform)
    {
        __private->userSetTransform = generateChange;
        __private->selectedTransform = newTransform;
        if (generateChange)
            connectChangeInterruptImpl(DBG_IOG_SOURCE_SET_TRANSFORM);
        else
        {
            __private->transform = newTransform;
            setProperty(kIOFBTransformKey, newTransform, 64);
        }
    }
    IOFB_END(setTransform,0,0,0);
}

UInt64 IOFramebuffer::getTransform( void )
{
    IOFB_START(getTransform,0,0,0);
    UInt64 tnsfrm = (__private->transform);
    IOFB_END(getTransform,tnsfrm,0,0);
    return (tnsfrm);
}

IOReturn IOFramebuffer::selectTransform( UInt64 transform, bool generateChange )
{
    IOFB_START(selectTransform,transform,generateChange,0);
    IOFramebuffer * next;
    next = this;
//    do
    {
        next->setTransform(transform, generateChange);
    }
//    while ((next = next->getNextDependent()) && (next != this));

    IOFB_END(selectTransform,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}


/*! Number of displays currently connected, regardless of online/offline or
 *  power state. */
uint32_t IOFramebuffer::globalConnectionCount()
{
    IOFramebuffer *fb = NULL;
    uint32_t count = 0;
    uintptr_t connectEnabled = 0;
    IOReturn err; (void) err;

    SYSASSERTGATED();

    FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers) {
        FBGATEGUARD(ctrlgated, fb);

        FB_START(getAttributeForConnection,kConnectionCheckEnable,__LINE__,0);
        err = fb->getAttributeForConnection(0, kConnectionCheckEnable, &connectEnabled);
        IOG_KTRACE(DBG_IOG_CONNECTION_ENABLE_CHECK, DBG_FUNC_NONE,
                   0, DBG_IOG_SOURCE_GLOBAL_CONNECTION_COUNT,
                   0, __private->regID,
                   0, connectEnabled,
                   0, err);
        FB_END(getAttributeForConnection,err,__LINE__,connectEnabled);
        if (!err && connectEnabled) count++;
    }

    return count;
}

/*!
 * Determine if this framebuffer should change its online/offline state based
 * on current framebuffer online state and lid state.
 *
 * The rules are:
 *   - Must be the internal panel.
 *   - At least one external display must be present.
 *   - If online and clamshell is closed, go offline.
 *     If offline and clamshell is opened, go online.
 *   - Must not be captured.
 *   - Must not be muted. On mux systems, we only want to operate on the
 *     currently active GPU.
 *
 * @return true if online state should change due to clamshell state.
 */
bool IOFramebuffer::clamshellOfflineShouldChange(bool online)
{
    uint32_t connectCount = -1;
    int reject = 0;
#if DEBG_CATEGORIES_BUILD
    const char *on = online ? "online" : "offline";
    const char *reason = "unknown";
    #define REJECT(why) do { reason = why; reject = __LINE__; } while (0)
#else
    #define REJECT(why) do {               reject = __LINE__; } while (0)
#endif

    IOFB_START(clamshellOfflineShouldChange,__private->regID,online,0);
    SYSASSERTGATED();

    if (kIOGDbgNoClamshellOffline & atomic_load(&gIOGDebugFlags)) {
        REJECT("kIOGDbgNoClamshellOffline");
    } else if (!__private->fBuiltInPanel) {
        // only state of builtin panel is affected by clamshell
        REJECT("not builtin");
    } else if (captured) {
        // capture defers connection changes
        REJECT("captured");
    } else if (__private->controller->isMuted()) {
        // in mux systems, only the unmuted fb should report a change
        REJECT("muted");
    } else if (!gIOFBCurrentClamshellState && online) {
        // clamshell opened, but already online
        REJECT("open + online");
    } else if (!gIOFBCurrentClamshellState && !online && !gIOFBLidOpenMode) {
        // clamshell opened, offline, but in legacy mode (builtin stays off)
        REJECT("open + offline + LidOpenMode");
    } else {
        connectCount = globalConnectionCount();

        if (gIOFBCurrentClamshellState && (connectCount > 1) && !online) {
            // clamshell closed, externals present, but already offline
            REJECT("closed + externals + offline");
        } else if (gIOFBCurrentClamshellState && (1 >= connectCount) && online) {
            // clamshell closed, externals not present, but already online
            REJECT("closed + no externals + online");
        }
    }

#undef REJECT

    if (reject)
    {
        D(GENERAL, thisName, " clamshellOfflineShouldChange? no: %s, currently: %s\n", reason, on);
    }
    else
    {
        D(GENERAL, thisName, " clamshellOfflineShouldChange? yes, currently: %s\n", on);
    }

    const uint64_t state =
        ((kIOGDbgNoClamshellOffline & atomic_load(&gIOGDebugFlags))
                                                      ? (1ULL <<  0) : 0) |
        ((__private->fBuiltInPanel)                   ? (1ULL <<  1) : 0) |
        ((captured)                                   ? (1ULL <<  2) : 0) |
        ((__private->controller->isMuted())           ? (1ULL <<  3) : 0) |
        ((gIOFBCurrentClamshellState)                 ? (1ULL <<  4) : 0) |
        ((online)                                     ? (1ULL <<  5) : 0) |
        ((gIOFBLidOpenMode)                           ? (1ULL <<  6) : 0) |
        static_cast<uint64_t>(connectCount) << 32;
    IOG_KTRACE(DBG_IOG_CLAMSHELL, DBG_FUNC_NONE,
               0, DBG_IOG_SOURCE_CLAMSHELL_OFFLINE_CHANGE,
               0, __private->regID, 0, reject, 0, state);
    IOFB_END(clamshellOfflineShouldChange,reject,0,0);
    return !reject;
}

IOReturn IOFramebuffer::probeAll( IOOptionBits options )
{
    IOFB_START(probeAll,options,0,0);
    IOReturn err = kIOReturnSuccess;

    D(GENERAL, "IOFB", " options=%#x\n", options);

    do
    {
        IOFramebuffer * fb;

        if (gIOGraphicsControl)
        {
            err = gIOGraphicsControl->requestProbe(options);
            D(GENERAL, "IOFB", " gIOGraphicsControl->requestProbe status=%#x\n", err);
        }

        FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
        {
            bool probed = false;
            FBGATEGUARD(ctrlgated, fb);

            if (!fb->captured)
            {
                const bool bOnline = fb->__private->online && !fb->__private->bClamshellOffline;
                if (fb->clamshellOfflineShouldChange(bOnline))
                {
                    fb->__private->bClamshellOffline = bOnline;
                    fb->connectChangeInterruptImpl(DBG_IOG_SOURCE_CLAMSHELL_OFFLINE_CHANGE);
                }
                else if (!gIOGraphicsControl || !fb->__private->controller->fAliasID)
                {
                    probed = true;
                    FB_START(setAttributeForConnection,kConnectionProbe,__LINE__,options);
                    err = fb->setAttributeForConnection(0, kConnectionProbe, options);
                    FB_END(setAttributeForConnection,err,__LINE__,0);
                }
            }
            D(GENERAL, "IOFB", " probed=%d err=%#x (captured=%d fAliasID=%#x)\n",
                probed, err, fb->captured, fb->__private->controller->fAliasID);
        }
    }
    while (false);

    IOFB_END(probeAll,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::requestProbe( IOOptionBits options )
{
    IOFB_START(requestProbe,options,0,0);
    IOReturn err;

    D(GENERAL, thisName, " requestProbe(%#x)\n", options);

    if (!gIOFBSystemWorkLoop || gIOFBSystemWorkLoop->inGate())
    {
        IOFB_END(requestProbe,kIOReturnNotReady,0,0);
        return (kIOReturnNotReady);
    }

    if ((err = extEntry(true, kIOGReportAPIState_RequestProbe)))
    {
        IOFB_END(requestProbe,err,__LINE__,0);
        return (err);
    }

#if 0
    if (!__private->online)
    {
		inst->extExit(err, kIOGReportAPIState_RequestProbe);
        IOFB_END(requestProbe,kIOReturnSuccess,__LINE__,0);
        return (kIOReturnSuccess);
    }
#endif

    if (kIOFBSetTransform & options)
    {
        options >>= 16;
        selectTransform(options, true);
    }
    else
    {
        if (captured)
        {
            err = kIOReturnBusy;
        }
        else
        {
            AbsoluteTime now;
            AbsoluteTime_to_scalar(&now) = mach_absolute_time();
            if (CMP_ABSOLUTETIME(&now, &gIOFBNextProbeAllTime) >= 0) 
            {
                triggerEvent(kIOFBEventProbeAll);
                clock_interval_to_deadline(10, kSecondScale, &gIOFBNextProbeAllTime);
            }
        }
    }

    extExit(err, kIOGReportAPIState_RequestProbe);

    IOFB_END(requestProbe,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

void IOFramebuffer::initFB(void)
{
    IOFB_START(initFB,0,0,0);

	if (!__private->online)
	{
		__private->needsInit = false;
        IOFB_END(initFB,0,__LINE__,0);
		return;
	}
	if (!fFrameBuffer)
    {
        IOFB_END(initFB,0,__LINE__,0);
        return;
    }

	do
	{
		IOReturn             err;
		IODisplayModeID      mode = 0;
		IOIndex              depth = 0;
		IOPixelInformation	 pixelInfo;
		IOMemoryDescriptor * fbRange;
		uint32_t 			 totalWidth, consoleDepth;
		uint8_t				 logo;
		bool				 timeout = false;

        FB_START(getCurrentDisplayMode,0,__LINE__,0);
		err = getCurrentDisplayMode(&mode, &depth);
        FB_END(getCurrentDisplayMode,err,__LINE__,0);
        IOG_KTRACE(DBG_IOG_GET_CURRENT_DISPLAY_MODE, DBG_FUNC_NONE,
                   0, DBG_IOG_SOURCE_INIT_FB,
                   0, __private->regID,
                   0, GPACKUINT32T(1, depth) | GPACKUINT32T(0, mode),
                   0, err);

		if (kIOReturnSuccess != err)
			break;
        FB_START(getPixelInformation,0,__LINE__,0);
		err = getPixelInformation(mode, depth, kIOFBSystemAperture, &pixelInfo);
        FB_END(getPixelInformation,err,__LINE__,0);
		if (kIOReturnSuccess != err)
			break;
		if (pixelInfo.activeWidth < 128)
			break;
	
        if (!fVramMap)
        {
            fbRange = getApertureRangeWithLength(kIOFBSystemAperture, pixelInfo.bytesPerRow * pixelInfo.activeHeight);
            if (!fbRange)
            {
                break;
            }
            fVramMap = fbRange->map(kIOFBMapCacheMode);
            fbRange->release();
            if (!fVramMap)
            {
                break;
            }
        }
        if (pixelInfo.bitsPerComponent > 8)
			consoleDepth = pixelInfo.componentCount * pixelInfo.bitsPerComponent;
		else
			consoleDepth = pixelInfo.bitsPerPixel;
		totalWidth = (pixelInfo.bytesPerRow * 8) / pixelInfo.bitsPerPixel;

// This code is never executed
//		if (false && __private->needsInit == 1)
//		{
//			const auto then
//			    = AbsoluteTime_to_scalar(&__private->controller->fInitTime);
//			const auto deltans = at2ns(mach_absolute_time() - then;
//			timeout = (deltans > kInitFBTimeoutNS);
//			if (timeout) DEBG1(thisName, " init timeout\n");
//		}
		// timeout = false;
		logo = ((!timeout) 
			&& (NULL != getProperty("AAPL,boot-display", gIOServicePlane))
			&& (__private->needsInit != 3));
		// logo = 1;
		if (logo)
		{
			PE_Video consoleInfo;
            IOService::getPlatform()->getConsoleInfo(&consoleInfo);
			if ((consoleInfo.v_width == pixelInfo.activeWidth)
				&& (consoleInfo.v_height == pixelInfo.activeHeight))
			{
				if (2 == consoleInfo.v_scale) logo = 2;
			}
			else
            {
                logo = 0;
                __private->needsInit = true;
            }
		}
		DEBG1(thisName, " initFB: needsInit %d logo %d\n", 
				__private->needsInit, logo);
		if (gIOFBVerboseBoot || ((2 != __private->needsInit) && (!gIOFBBlackBootTheme)))
		{
			IOFramebufferBootInitFB(
				fVramMap->getVirtualAddress(),
				pixelInfo.activeWidth, pixelInfo.activeHeight, 
				totalWidth, consoleDepth, 
				logo);
			if (logo) __private->refreshBootGraphics = true;
		}
		DEBG1(thisName, " initFB: done\n");
		bool bootGamma = getProperty(kIOFBBootGammaRestoredKey, gIOServicePlane);
		if (logo && !bootGamma && !gIOFBBlackBootTheme)
        {
            updateGammaTable(3,
                             256,
                             16,
                             NULL,
                             kIOFBSetGammaSyncNotSpecified,
                             false, /*immediate*/
                             false /*ignoreTransactionActive*/);
        }
		__private->needsInit = false;
		setProperty(kIOFBNeedsRefreshKey, (0 == logo));
		setProperty(kIOFBBootGammaRestoredKey, bootGamma ? gIOFBOne32Data : gIOFBZero32Data);
	}
	while (false);

    IOFB_END(initFB,0,0,0);
}

IOReturn IOFramebuffer::postOpen( void )
{
    IOFB_START(postOpen,0,0,0);
	OSObject * obj;
    OSBoolean * osb;

	__private->needsInit = true;
	obj = getProperty(kIOFBNeedsRefreshKey);
    if (NULL != obj)
    {
        osb = OSDynamicCast(OSBoolean, obj);
        if (NULL != osb)
        {
            __private->needsInit += osb->isFalse();
        }
    }
	setProperty(kIOFBNeedsRefreshKey, true);
	initFB();

    if (__private->cursorAttributes)
    {
        __private->cursorAttributes->release();
        __private->cursorAttributes = 0;
    }

    __private->cursorAttributes = OSArray::withCapacity(2);
    if (!__private->cursorAttributes)
    {
        IOFB_END(postOpen,kIOReturnNoMemory,0,0);
        return (kIOReturnNoMemory);
    }

    __private->testingCursor = true;

    FB_START(setCursorImage,0,__LINE__,0);
    setCursorImage( (void *) 0 );
    FB_END(setCursorImage,0,__LINE__,0);

    if (__private->cursorThread)
    {
        IOHardwareCursorDescriptor desc;

        desc.majorVersion       = kHardwareCursorDescriptorMajorVersion;
        desc.minorVersion       = kHardwareCursorDescriptorMinorVersion;
        desc.height             = 256;
        desc.width              = 256;
        desc.bitDepth           = 32;
        desc.maskBitDepth       = 0;
        desc.colorEncodings     = 0;
        desc.flags              = 0;
        desc.supportedSpecialEncodings = kTransparentEncodedPixel;

        (*__private->cursorControl.callouts->setCursorImage) (
            __private->cursorControl.inst, __private->cursorControl.ref,
            &desc, (void *) 0 );

        if (__private->controller->fWl)
            __private->controller->fWl->addEventSource(__private->cursorThread);
    }

    __private->testingCursor = false;

    setProperty( kIOFBCursorInfoKey, __private->cursorAttributes );

    IOService * sensor = 0;
    uintptr_t value[16];

//#define kTempAttribute        kConnectionWSSB
#define kTempAttribute  'thrm'

    FB_START(getAttributeForConnection,kTempAttribute,__LINE__,0);
    IOReturn err = getAttributeForConnection(0, kTempAttribute, &value[0]);
    FB_END(getAttributeForConnection,err,__LINE__,0);
    if (!__private->temperatureSensor && (kIOReturnSuccess == err))
    do
    {
        UInt32     data;
        OSNumber * num;

        num = OSDynamicCast(OSNumber, getProperty(kIOFBDependentIDKey));
        if (num && num->unsigned32BitValue())
            continue;

        sensor = new IOService;
        if (!sensor)
            continue;
        if (!sensor->init())
            continue;

#define kTempSensorName         "temp-sensor"
        sensor->setName(kTempSensorName);
        sensor->setProperty("name", (void *) kTempSensorName, static_cast<unsigned int>(strlen(kTempSensorName) + 1));
        sensor->setProperty("compatible", (void *) kTempSensorName, static_cast<unsigned int>(strlen(kTempSensorName) + 1));
        sensor->setProperty("device_type", (void *) kTempSensorName, static_cast<unsigned int>(strlen(kTempSensorName) + 1));
        sensor->setProperty("type", "temperature");
        sensor->setProperty("location", "GPU");
        data = 0xff000002;
        sensor->setProperty("zone", &data, sizeof(data));
        data = 0x00000001;
        sensor->setProperty("version", &data, sizeof(data));

        OSData * prop;
        IOService * device;
        data = 0x12345678;
        if ((device = getProvider())
         && (prop = OSDynamicCast(OSData, device->getProperty("AAPL,phandle"))))
            data = (*((UInt32 *) prop->getBytesNoCopy())) << 8;
        sensor->setProperty("sensor-id", &data, sizeof(data));

        if (!sensor->attach(this))
            continue;

        sensor->registerService();
        __private->temperatureSensor = sensor;
    }
    while (false);

    if (sensor)
        sensor->release();

    IOFB_END(postOpen,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::callPlatformFunction( const OSSymbol * functionName,
                                                    bool waitForFunction,
                                                    void *p1, void *p2,
                                                    void *p3, void *p4 )
{
    IOFB_START(callPlatformFunction,waitForFunction,0,0);
    uintptr_t   value[16];
    IOReturn ret;

    if (functionName != gIOFBGetSensorValueKey)
    {
        ret = (super::callPlatformFunction(functionName, waitForFunction, p1, p2, p3, p4));
        IOFB_END(callPlatformFunction,ret,__LINE__,0);
        return (ret);
    }

    FBGATEGUARD(ctrlgated, this);  // Take controller lock

    FB_START(getAttributeForConnection,kTempAttribute,__LINE__,0);
    ret = getAttributeForConnection(0, kTempAttribute, &value[0]);
    FB_END(getAttributeForConnection,ret,__LINE__,0);

    if (kIOReturnSuccess == ret)
        *((UInt32 *)p2) = static_cast<UInt32>(((value[0] & 0xffff) << 16));

    IOFB_END(callPlatformFunction,ret,0,0);
    return (ret);
}

IOReturn IOFramebuffer::message(UInt32 type, IOService *provider, void *argument)
{
    IOFB_START(message,type,0,0);
#pragma unused(argument)
    IOReturn status = kIOReturnUnsupported;
    DEBG1(thisName, "(%#x, %p, %p)\n", type, provider, argument);
    switch (type) {
        case kIOMessageGraphicsNotifyTerminated: {
            SYSGATEGUARD(sysgated);
            FBGATEGUARD(ctrlgated, this);
            status = deliverFramebufferNotification(kIOFBNotifyTerminated);
            break;
        }
        case kIOMessageGraphicsProbeAccelerator: {
            SYSGATEGUARD(sysgated);
            if (__private && __private->controller) {
                FBGATEGUARD(ctrlgated, this);
                status = probeAccelerator();
            } else {
                // rdar://36053553
                status = probeAccelerator();
            }
            break;
        }
    }
    DEBG1(thisName, "-> %#x\n", status);
    IOFB_END(message,status,0,0);
    return status;
}

IOWorkLoop * IOFramebuffer::getControllerWorkLoop() const
{
    IOFB_START(getControllerWorkLoop,0,0,0);
    IOWorkLoop * wl = __private->controller->fWl;
    IOFB_END(getControllerWorkLoop,0,0,0);
	return (wl);
}

IOWorkLoop * IOFramebuffer::getGraphicsSystemWorkLoop() const
{
    IOFB_START(getGraphicsSystemWorkLoop,0,0,0);
    IOWorkLoop * wl = (gIOFBSystemWorkLoop);
    IOFB_END(getGraphicsSystemWorkLoop,0,0,0);
    return (wl);
}

IOWorkLoop * IOFramebuffer::getWorkLoop() const
{
    IOFB_START(getWorkLoop,0,0,0);
    IOWorkLoop * wl = NULL;
	if (__private && __private->controller)
        wl = __private->controller->fWl;
    IOFB_END(getWorkLoop,0,0,0);
    return (wl);
}

void IOFramebuffer::setCaptured( bool isCaptured )
{
    IOFB_START(setCaptured,isCaptured,0,0);
    FCASSERTGATED(__private->controller);

    bool wasCaptured = captured;

    captured = isCaptured;

	DEBG1(thisName, " captured %d -> %d\n", wasCaptured, captured);

    if (wasCaptured != isCaptured)
    {
        if (isCaptured)
            setProperty(kIOFBCapturedKey, kOSBooleanTrue);
        else
            removeProperty(kIOFBCapturedKey);
		deliverFramebufferNotification(kIOFBNotifyCaptureChange, (void *) isCaptured);

		__private->controller->startThread();
    }
    IOFB_END(setCaptured,0,0,0);
}

void IOFramebuffer::setDimDisable( bool dimDisable )
{
    IOFB_START(setDimDisable,dimDisable,0,0);
    __private->dimDisable = dimDisable;
    IOFB_END(setDimDisable,0,0,0);
}

bool IOFramebuffer::getDimDisable( void )
{
    IOFB_START(getDimDisable,0,0,0);
    bool b = (__private->dimDisable);
    IOFB_END(getDimDisable,b,0,0);
    return (b);
}

void IOFramebuffer::setNextDependent( IOFramebuffer * dependent )
{
    IOFB_START(setNextDependent,0,0,0);
    nextDependent = dependent;
    IOFB_END(setNextDependent,0,0,0);
}

IOFramebuffer * IOFramebuffer::getNextDependent( void )
{
    IOFB_START(getNextDependent,0,0,0);
    IOFB_END(getNextDependent,0,0,0);
    return (nextDependent);
}

bool IOFramebuffer::initNotifiers(void)
{
    IOFB_START(initNotifiers,0,0,0);

    bool            bRet = true;
    OSOrderedSet    * notifySet = NULL;
    unsigned int    groupIndex = 0;

    if (NULL == fFBNotifications)
    {
        fFBNotifications = OSArray::withCapacity(kIOFBNotifyGroupIndex_NumberOfGroups);
        if (NULL != fFBNotifications)
        {
            for (groupIndex = 0; groupIndex < kIOFBNotifyGroupIndex_NumberOfGroups; groupIndex++ )
            {
                notifySet = OSOrderedSet::withCapacity( 1, &IOFramebuffer::osNotifyOrderFunction, (void *)this );
                if (NULL != notifySet)
                {
                    if (true != fFBNotifications->setObject(groupIndex, notifySet))
                    {
                        DEBG1(thisName, " Failed to set notification orderedset for index: %u!\n", groupIndex);
                        bRet = false;
                        break;
                    }
                }
                else
                {
                    DEBG1(thisName, " Failed to allocate notification orderedset for index: %u!\n", groupIndex);
                    bRet = false;
                    break;
                }
            }
        }
        else
        {
            DEBG1(thisName, " Failed to allocate notification array!\n");
            bRet = false;
        }
    }

    IOFB_END(initNotifiers,bRet,0,0);
    return (bRet);
}

void IOFramebuffer::disableNotifiers( void )
{
    IOFB_START(disableNotifiers,0,0,0);
    OSOrderedSet            * notifySet = NULL;
    _IOFramebufferNotifier  * notify = NULL;
    unsigned int            groupIndex = 0;
    unsigned int            notifyIndex = 0;

    // Disable all notification callouts
    for (groupIndex = 0; groupIndex < kIOFBNotifyGroupIndex_NumberOfGroups; groupIndex++)
    {
        notifySet = (OSOrderedSet *)fFBNotifications->getObject(gReverseGroup[groupIndex]);
        if (NULL != notifySet)
        {
            notifyIndex = notifySet->getCount();
            while (notifyIndex > 0)
            {
                notifyIndex--;
                notify = (_IOFramebufferNotifier *)notifySet->getObject(notifyIndex);
                if (NULL != notify)
                {
                    notify->disable();
                }
            }
        }
    }

    IOFB_END(disableNotifiers,0,0,0);
}

void IOFramebuffer::cleanupNotifiers(void)
{
    IOFB_START(cleanupNotifiers,0,0,0);
    OSOrderedSet            * notifierSet = NULL;
    unsigned int            notifierCount = 0;

    if (NULL != fFBNotifications)
    {
        disableNotifiers();

        notifierCount = fFBNotifications->getCount();
        while (notifierCount > 0)
        {
            notifierCount--;
            notifierSet = (OSOrderedSet *)fFBNotifications->getObject(notifierCount);
            if (NULL != notifierSet)
            {
                notifierSet->flushCollection();
            }
        }
        fFBNotifications->flushCollection();
        OSSafeReleaseNULL(fFBNotifications);
    }
    IOFB_END(cleanupNotifiers,0,0,0);
}

bool IOFramebuffer::isVendorDevicePresent(unsigned int type, bool bMatchInteralOnly)
{
    IOFramebuffer   * fb;
    uint8_t         nvCount = 0;
    uint8_t         nvExtCount = 0;
    uint8_t         amdCount = 0;
    uint8_t         amdExtCount = 0;
    uint8_t         igCount = 0;
    uint8_t         igExtCount = 0;

    bool            bHasVendorDeviceType = false;

    FORALL_FRAMEBUFFERS(fb, /* in */ gAllFramebuffers)
    {
        FBGATEGUARD(ctrlgated, fb);
        switch (fb->__private->controller->fVendorID)
        {
            case kPCI_VID_INTEL:
            case kPCI_VID_APPLE:
                if (fb->__private->controller->fExternal) {
                    igExtCount++;
                } else {
                    igCount++;
                }
                break;
            case kPCI_VID_AMD:
            case kPCI_VID_AMD_ATI:
                if (fb->__private->controller->fExternal) {
                    amdExtCount++;
                } else {
                    amdCount++;
                }
                break;
            case kPCI_VID_NVIDIA:
            case kPCI_VID_NVIDIA_AGEIA:
                if (fb->__private->controller->fExternal) {
                    nvExtCount++;
                } else {
                    nvCount++;
                }
                break;
            default:
                break;
        }
    }

    switch (type)
    {
        case kVendorDeviceIntel:
            bHasVendorDeviceType = bMatchInteralOnly ? igCount > 0 : (igCount > 0) || (igExtCount > 0);
            break;
        case kVendorDeviceAMD:
            bHasVendorDeviceType = bMatchInteralOnly ? amdCount > 0 : (amdCount > 0) || (amdExtCount > 0);
            break;
        case kVendorDeviceNVidia:
            bHasVendorDeviceType = bMatchInteralOnly ? nvCount > 0 : (nvCount > 0) || (nvExtCount > 0);
            break;
        default:
            break;
    }

    return (bHasVendorDeviceType);
}

bool IOFramebuffer::fillFramebufferBlack( void )
{
    bool    bFillCompleted = false;

    if ((NULL != fVramMap) &&
        (NULL != fFrameBuffer)) {
        const uint64_t requiredLen = rowBytes *
                                     __private->framebufferHeight;
        const uint64_t mapLen = fVramMap->getLength();
        if (mapLen >= requiredLen) {
            volatile unsigned char * line = fFrameBuffer;
            const auto numbytes = __private->framebufferWidth *
                                  __private->cursorBytesPerPixel;
            for (uint32_t y = 0; y < __private->framebufferHeight; y++) {
                bzero((void *) line, numbytes);
                line += rowBytes;
            }
            bFillCompleted = true;
        }
    }

    return (bFillCompleted);
}

void IOFramebuffer::extClose(void)
{
    IOG_KTRACE(DBG_IOG_FB_EXT_CLOSE, DBG_FUNC_START,
               0, __private->regID, 0, 0, 0, 0, 0, 0);

    // IOFramebufferUserClient::stop() on provider's WL
    // (IOFramebuffer::getWorkLoop).
    SYSASSERTNOTGATED();
    FBASSERTGATED(this);

    // Start closeWork() on sys WL and sleep (with gate open) until it finishes.
    __private->fClosePending = true;
    do {
        __private->fCloseWorkES->interruptOccurred(0, 0, 0);
        FBWL(this)->sleepGate(&__private->fClosePending, THREAD_UNINT);
    } while (__private->fClosePending);

    IOG_KTRACE(DBG_IOG_FB_EXT_CLOSE, DBG_FUNC_END,
               0, __private->regID, 0, 0, 0, 0, 0, 0);
}

void IOFramebuffer::closeWork(IOInterruptEventSource *, int)
{
    SYSASSERTGATED(); // Scheduled on sys WL by extClose.
    FBGATEGUARD(ctrlgated, this);
    close();
    __private->fClosePending = false;
    FBWL(this)->wakeupGate(&__private->fClosePending, kManyThreadWakeup);
}

/*!
 * Places the framebuffer into a WindowServer-less mode of operation (i.e.,
 * unaccelerated, maybe used as console).
 */
void IOFramebuffer::close( void )
{
    unsigned int        idx;
    mach_msg_header_t  *msgh;

    IOG_KTRACE(DBG_IOG_FB_CLOSE, DBG_FUNC_START,
               0, __private->regID, 0, 0, 0, 0, 0, 0);

    SYSASSERTGATED();
    FBASSERTGATED(this);

    DEBG1(thisName, "\n");

    // <rdar://problem/32315292> Lobo: Apple Logo + progress bar shown briefly when shutting down
    // Only send the WSAA close event for managed FBs, else IOAF/drivers get confused.
    if (kIOWSAA_DriverOpen != __private->wsaaState)
    {
        setWSAAAttribute(kIOWindowServerActiveAttribute, (uint32_t)(kIOWSAA_Unaccelerated | (__private->wsaaState & kIOWSAA_NonConsoleDevice)));
        __private->wsaaState = kIOWSAA_DriverOpen;
    }
    // <rdar://problem/32880536> Intel: (J45G/IG) Flash of color logging out or rebooting
    else if (!(kIOGDbgRemoveShutdownProtection & gIOGDebugFlags)) {
        if (isVendorDevicePresent(kVendorDeviceNVidia, true/*bMatchInteralOnly*/)
            && (this == gIOFBConsoleFramebuffer)
            && __private->controller->fIntegrated) {
            IOReturn ret = kIOReturnError;
            bool bFilled = false;

            // For verbose, try to set the pixels to black and fallback to gamma
            if (gIOFBVerboseBoot) {
                bFilled = fillFramebufferBlack();
            }

            if (!bFilled) {
                // For gamma, if not present or buffer allocation failure,
                // fallback to pixel black
                // If both fail, nothing we can do
                if (__private->gammaDataLen) {
                    UInt8 * gammaData = IONew(UInt8, __private->gammaDataLen);
                    if (NULL != gammaData) {
                        bzero(gammaData, sizeof(UInt8) * __private->gammaDataLen);

                        IOG_KTRACE(DBG_IOG_SET_GAMMA_TABLE, DBG_FUNC_START,
                                   0, __private->regID,
                                   0, DBG_IOG_SOURCE_CLOSE, 0, 0, 0, 0);
                        ret = setGammaTable(__private->gammaChannelCount,
                                            __private->gammaDataCount,
                                            __private->gammaDataWidth,
                                            gammaData, false);
                        IOG_KTRACE(DBG_IOG_SET_GAMMA_TABLE, DBG_FUNC_END,
                                   0, __private->regID,
                                   0, DBG_IOG_SOURCE_CLOSE,
                                   0, ret, 0, 0);

                        IODelete(gammaData, UInt8, __private->gammaDataLen);

                        if (kIOReturnSuccess == ret) {
                            bFilled = true;
                        }
                    } else {
                        bFilled = fillFramebufferBlack();
                    }
                } else {
                    bFilled = fillFramebufferBlack();
                }
            }

            if (!bFilled) {
                IOLog("Failed logout/shutdown fill (%#x)\n", ret );
            }
        }
    }

    /* <rdar://problem/31526474> J45g IG - Logging out causes system hang (sometimes WindowServer crash) with change from IOGraphics-203b8
     Do not disable or delete any notifiers at this time.  Kernel components don't "listen" for IOFBs to come and go
     Therefore we cannot delete or disable these notifications until stop()
     */
    //cleanupNotifiers();

    if ((this == gIOFBConsoleFramebuffer) &&
        getPowerState() &&
        (0 == (__private->timingInfo.flags & kDisplayModeAcceleratorBackedFlag)) )
    {
        setPlatformConsole(0, kPEAcquireScreen, DBG_IOG_SOURCE_CLOSE);
    }

    __private->gammaSyncType = kIOFBSetGammaSyncNotSpecified;
    resetLimitState();

    msgh = (mach_msg_header_t *) serverMsg;
    if (msgh)
        msgh->msgh_remote_port = MACH_PORT_NULL;

    __private->controller->fWsWait |= (1 << __private->controllerIndex);
    __private->fServerUsesModernAcks = false;
    captured = false;
    setProperty(kIOFBNeedsRefreshKey, true);

    for (idx = 0;
         (idx < kIOPreviewImageCount) && __private->saveBitsMD[idx];
         idx++)
    {
        __private->saveBitsMD[idx]->release();
        __private->saveBitsMD[idx] = 0;
    }

    // We depend on fServerConnect remaining set until close() is finished to
    // prevent another client from connecting while this one is still closing.
    fServerConnect = NULL;

    IOG_KTRACE(DBG_IOG_FB_CLOSE, DBG_FUNC_END,
               0, __private->regID, 0, 0, 0, 0, 0, 0);
}

// <rdar://problem/27591351> Somewhat frequent hangs(panic) when sleeping and subsequently waking macbook on 16A272a.
IODeviceMemory * IOFramebuffer::getApertureRangeWithLength( IOPixelAperture aperture, IOByteCount requiredLength )
{
    IOFB_START(getApertureRangeWithLength,aperture,requiredLength,0);
    IODeviceMemory      * fbRange = NULL;
    IOByteCount         fbRangeLength = 0;

    FB_START(getApertureRange,0,__LINE__,0);
    fbRange = getApertureRange(kIOFBSystemAperture);
    FB_END(getApertureRange,0,__LINE__,0);
    if (NULL != fbRange)
    {
        fbRangeLength = fbRange->getLength();
        // Make sure the vendor provided some length in the descriptor
        if (0 == fbRangeLength)
        {
            if (kIOGDbgFBRange & gIOGDebugFlags)
            {
                panic("%s:%#llx - VENDOR_BUG: getApertureRange(kIOFBSystemAperture) length is zero!!\n", thisName, __private->regID);
            }
            else
            {
                IOLog("%s:%#llx - VENDOR_BUG: getApertureRange(kIOFBSystemAperture) length is zero!!\n", thisName, __private->regID);
            }

            fbRange->release();
            fbRange = NULL;
        }
        // Make sure the vendor provided enough length in the descriptor for the current buffer
        else if (requiredLength > fbRangeLength)
        {
            if (kIOGDbgFBRange & gIOGDebugFlags)
            {
                panic("%s:%#llx - VENDOR_BUG: getApertureRange(kIOFBSystemAperture) length insufficient.  Required: %llu Have: %llu!!\n", thisName, __private->regID, requiredLength, fbRangeLength);
            }
            else
            {
                IOLog("%s:%#llx - VENDOR_BUG: getApertureRange(kIOFBSystemAperture) length insufficient.  Required: %llu Have: %llu!!\n", thisName, __private->regID, requiredLength, fbRangeLength);
            }

            fbRange->release();
            fbRange = NULL;
        }
    }

    IOFB_END(getApertureRangeWithLength,0,0,0);
    return (fbRange);
}

IODeviceMemory * IOFramebuffer::getVRAMRange( void )
{
    IOFB_START(getVRAMRange,0,0,0);
    IODeviceMemory * dm = getApertureRangeWithLength(kIOFBSystemAperture, __private->pixelInfo.bytesPerRow * __private->pixelInfo.activeHeight);
    IOFB_END(getVRAMRange,0,0,0);
    return (dm);
}

IOReturn IOFramebuffer::setUserRanges( void )
{
    IOFB_START(setUserRanges,0,0,0);
#if RLOG
    // print ranges
    uint32_t              i, numRanges;
    IOMemoryDescriptor *    mem;
    numRanges = userAccessRanges->getCount();
    DEBG(thisName, " ranges num:%d\n", numRanges);
    for (i = 0; i < numRanges; i++)
    {
        mem = (IOMemoryDescriptor *) userAccessRanges->getObject( i );
        if (0 == mem)
            continue;
        DEBG(thisName, " start:%llx size:%lx\n",
             mem->getPhysicalSegment(0, 0, kIOMemoryMapperNone), (long) mem->getLength() );
    }
#endif

    IOFB_END(setUserRanges,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::setBackingFramebuffer(const IOPixelInformation * info,
					      uint32_t bufferCount,
					      void * mappedAddress[])
{
    IOFB_START(setBackingFramebuffer,bufferCount,0,0);
    IOFB_END(setBackingFramebuffer,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::switchBackingFramebuffer(uint32_t bufferIndex)
{
    IOFB_START(switchBackingFramebuffer,bufferIndex,0,0);
    IOFB_END(switchBackingFramebuffer,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

/* static */ void IOFramebuffer::findConsole(void)
{
    IOFB_START(findConsole,0,0,0);
    PE_Video newConsole;
    IOFramebuffer * look;
    IOFramebuffer * fb = NULL;
    uintptr_t       value;

    FORALL_FRAMEBUFFERS(look, /* in */ gAllFramebuffers)
    {
        if (!look->__private
        ||  !look->fFrameBuffer
        ||  !look->__private->framebufferWidth
        ||  !look->__private->framebufferHeight
        ||  !look->__private->online
        ||  !look->__private->controller
        ||  !look->__private->controller->fDevice
        ||  !look->__private->consoleDepth
        ||   look->__private->consoleDepth > 32)
            continue;

        // Check if the console is ONLINE, if not skip to next console.
        FB_START(getAttribute,kIOVRAMSaveAttribute,__LINE__,0);
        IOReturn err = look->getAttribute(kIOVRAMSaveAttribute, &value);
        FB_END(getAttribute,err,__LINE__,0);
		if ((kIOReturnSuccess == err) && !value)
			continue;

        // Display has backlight, prefer this display over others?  Implies
        // there is a policy of internals are preferred to be console.
		if (kIODisplayOptionBacklight & look->__private->displayOptions)
		{
			fb = look;
			break;
		}
        // If all else fails, cache the first framebuffer found or use whatever
        // we last decided was the Console framebuffer.
        if (!fb || (look == gIOFBConsoleFramebuffer))
		{
			fb = look;
		}
    }

    if (fb)
    {
		DEBG1(fb->thisName, " console set 0x%x000 %d x %d\n",
							pmap_find_phys(kernel_pmap,
                                           (addr64_t) fb->fFrameBuffer),
							fb->__private->framebufferWidth,
							fb->__private->framebufferHeight);
        bzero(&newConsole, sizeof(newConsole));
        newConsole.v_baseAddr   = (unsigned long) fb->fFrameBuffer;
        newConsole.v_rowBytes   = fb->rowBytes;
        newConsole.v_width      = fb->__private->framebufferWidth;
        newConsole.v_height     = fb->__private->framebufferHeight;
        newConsole.v_depth      = fb->__private->consoleDepth;
        newConsole.v_scale      = fb->__private->uiScale;
        newConsole.v_display    = 1;  // graphics mode for i386
        fb->setPlatformConsole(&newConsole, kPEReleaseScreen,
                               DBG_IOG_SOURCE_FINDCONSOLE);
        fb->setPlatformConsole(&newConsole, kPEEnableScreen,
                               DBG_IOG_SOURCE_FINDCONSOLE);

#ifndef kPERefreshBootGraphics
#define kPERefreshBootGraphics	9
#endif
		if (fb->__private->refreshBootGraphics)
		{
			fb->__private->refreshBootGraphics = false;
			fb->setPlatformConsole(0, kPERefreshBootGraphics,
                                   DBG_IOG_SOURCE_FINDCONSOLE);
		}

        gIOFBConsoleFramebuffer = fb;
        DEBG1(fb->thisName, " now console\n");
    }
    IOFB_END(findConsole,0,0,0);
}

IOReturn IOFramebuffer::setupForCurrentConfig( void )
{
    IOFB_START(setupForCurrentConfig,0,0,0);
	TIMESTART();
	if (__private->paramHandler)
		__private->paramHandler->displayModeChange();
	TIMEEND(thisName, "paramHandler->displayModeChange time: %qd ms\n");

    IOReturn err = (doSetup(true));
    IOFB_END(setupForCurrentConfig,err,0,0);
    return (err);
}

OSData * IOFramebuffer::getConfigMode(IODisplayModeID mode, const OSSymbol * sym)
{
    IOFB_START(getConfigMode,mode,0,0);
	OSDictionary * dict;
	OSArray * array;
	OSNumber * num;
    OSData * dat;
	unsigned int idx;
	
	dict = OSDynamicCast(OSDictionary, getProperty(gIOFBConfigKey));
	if (!dict)
    {
        IOFB_END(getConfigMode,-1,__LINE__,0);
        return (NULL);
    }
	array = OSDynamicCast(OSArray, dict->getObject(gIOFBModesKey));
	if (!array)
    {
        IOFB_END(getConfigMode,-1,__LINE__,0);
        return (NULL);
    }
	for (idx = 0; (dict = OSDynamicCast(OSDictionary, array->getObject(idx))); idx++)
	{
		if (!(num = OSDynamicCast(OSNumber, dict->getObject(gIOFBModeIDKey)))) continue;
		if (num->unsigned32BitValue() == (UInt32) mode) break;
	}
	if (!dict)
    {
        IOFB_END(getConfigMode,-1,__LINE__,0);
        return (NULL);
    }
    dat = OSDynamicCast(OSData, dict->getObject(sym));
    IOFB_END(getConfigMode,0,0,0);
	return (dat);
}

void IOFramebuffer::setVBLTiming(void)
{
    IOFB_START(setVBLTiming,0,0,0);
    StdFBShmem_t * shmem = GetShmem(this);

	if (kIODetailedTimingValid & __private->timingInfo.flags)
	{
		uint64_t count = ((uint64_t)(__private->timingInfo.detailedInfo.v2.horizontalActive
								+ __private->timingInfo.detailedInfo.v2.horizontalBlanking));
		count *= ((uint64_t)(__private->timingInfo.detailedInfo.v2.verticalActive
							   + __private->timingInfo.detailedInfo.v2.verticalBlanking));
		if (kIOInterlacedCEATiming & __private->timingInfo.detailedInfo.v2.signalConfig)
			count >>= 1;

		uint64_t clock  = __private->timingInfo.detailedInfo.v2.pixelClock;
		uint64_t actual = __private->timingInfo.detailedInfo.v2.minPixelClock;

		DEBG1(thisName, " minPixelClock %qd maxPixelClock %qd\n", 
						__private->timingInfo.detailedInfo.v2.maxPixelClock,
						__private->timingInfo.detailedInfo.v2.minPixelClock);

		if (actual != __private->timingInfo.detailedInfo.v2.maxPixelClock)
			actual = 0;
		bool throttleEnable = (gIOFBVBLThrottle && clock && actual && shmem);
		DEBG1(thisName, " vblthrottle(%d) clk %qd act %qd\n", throttleEnable, clock, actual);
		if (throttleEnable)
			clock = actual;
		setProperty(kIOFBCurrentPixelClockKey, clock, 64);
		setProperty(kIOFBCurrentPixelCountRealKey, count, 64);
		setProperty(kIOFBCurrentPixelCountKey, (count * gIOFBVblDeltaMult) >> 16, 64);

 
		if (shmem && throttleEnable)
		{
			mach_timebase_info_data_t timebaseInfo;
			clock_timebase_info(&timebaseInfo);
			AbsoluteTime_to_scalar(&shmem->vblDeltaReal) 
					= (count * kSecondScale * timebaseInfo.numer / clock / timebaseInfo.denom);
			AbsoluteTime_to_scalar(&shmem->vblDelta) = (AbsoluteTime_to_scalar(&shmem->vblDeltaReal) * gIOFBVblDeltaMult) >> 16;
		}
		__private->vblThrottle = throttleEnable;
	}
	else
	{
		removeProperty(kIOFBCurrentPixelClockKey);
		removeProperty(kIOFBCurrentPixelCountKey);
		if (shmem && __private->vblThrottle)
		{
			AbsoluteTime_to_scalar(&shmem->vblDeltaReal) = 0;
			AbsoluteTime_to_scalar(&shmem->vblDelta) = 0;
		}
	}
    IOFB_END(setVBLTiming,0,0,0);
}

IOReturn IOFramebuffer::doSetup( const bool full )
{
    IOFB_START(doSetup,full,0,0);
    IOReturn                    err;
    IODisplayModeID             mode = 0;
    IOIndex                     depth = 0;
    IOMemoryDescriptor *        mem;
    IOMemoryDescriptor *        fbRange;
	OSData *				    data;
    IOPhysicalAddress64         base;
    uintptr_t                   value;
    bool                        haveFB = __private->online;

	bzero(&__private->pixelInfo, sizeof(__private->pixelInfo));
	if (haveFB)
	{
        FB_START(getAttribute,kIOHardwareCursorAttribute,__LINE__,0);
		err = getAttribute( kIOHardwareCursorAttribute, &value );
        FB_END(getAttribute,err,__LINE__,0);
		__private->cursorPanning = ((err == kIOReturnSuccess) && (0 != (kIOFBCursorPans & value)));

        FB_START(getCurrentDisplayMode,0,__LINE__,0);
        err = getCurrentDisplayMode( &mode, &depth );
        FB_END(getCurrentDisplayMode,err,__LINE__,0);
        IOG_KTRACE(DBG_IOG_GET_CURRENT_DISPLAY_MODE, DBG_FUNC_NONE,
                   0, DBG_IOG_SOURCE_DO_SETUP,
                   0, __private->regID,
                   0, GPACKUINT32T(1, depth) | GPACKUINT32T(0, mode),
                   0, err);
		if (kIOReturnSuccess == err)
        {
            FB_START(getPixelInformation,0,__LINE__,0);
			err = getPixelInformation( mode, depth, kIOFBSystemAperture, &__private->pixelInfo );
            FB_END(getPixelInformation,err,__LINE__,0);
        }
		if (kIOReturnSuccess != err)
			bzero(&__private->pixelInfo, sizeof(__private->pixelInfo));
		if (__private->pixelInfo.activeWidth < 128)
			haveFB = false;
		else if ((data = getConfigMode(mode, gIOFBModeDMKey)))
		{
			IODisplayModeInformation * info = (IOGRAPHICS_TYPEOF(info)) data->getBytesNoCopy();
			if (info->imageWidth)
			{
				if ((__private->pixelInfo.activeWidth >= 2048)
				 && (__private->pixelInfo.activeHeight >= 1280)
				 && (((254 * __private->pixelInfo.activeWidth) / info->imageWidth) > k2xDPI))
					 __private->uiScale  = 2;
				else __private->uiScale  = 1;
			}
			else __private->uiScale      = 0;
		}
	}

    __private->timingInfo.flags = kIODetailedTimingValid;
    FB_START(getTimingInfoForDisplayMode,mode,__LINE__,0);
    IOReturn kr = getTimingInfoForDisplayMode(mode, &__private->timingInfo);
    FB_END(getTimingInfoForDisplayMode,kr,__LINE__,0);
	if (!haveFB || (kIOReturnSuccess != kr))
	{
		bzero(&__private->timingInfo, sizeof(__private->timingInfo));
	}

    if (haveFB)
	{
		setVBLTiming();
		if (kIODetailedTimingValid & __private->timingInfo.flags) __private->setupMode = mode;
        __private->scaledMode = false;
        if (kIOScalingInfoValid & __private->timingInfo.flags)
        {
            const auto& i = __private->timingInfo.detailedInfo.v2; // alias
            __private->scaledMode = (
                    i.scalerFlags
                 || i.horizontalScaledInset || i.verticalScaledInset
                 || (i.horizontalScaled
                         && (i.horizontalScaled != i.horizontalActive))
                 || (i.verticalScaled
                         && (i.verticalScaled != i.verticalActive))
            );
        }
    }

    if (full)
    {
        const auto apertureLen = __private->pixelInfo.bytesPerRow
                               * __private->pixelInfo.activeHeight;
        OSSafeReleaseNULL(fVramMap);
        fFrameBuffer = NULL;
        fbRange = getApertureRangeWithLength(kIOFBSystemAperture, apertureLen);
    	if (NULL != fbRange)
        {
            userAccessRanges->removeObject( kIOFBSystemAperture );
            userAccessRanges->setObject( kIOFBSystemAperture, fbRange );
            err = setUserRanges();

            base = fbRange->getPhysicalSegment(0, 0, kIOMemoryMapperNone);
            // getApertureRangeWithLength() will have validated the result from
            // getVRAMRange() for IOFB based drivers, since both methods call
            // into getApertureRange() for the the same aperture type

            FB_START(getVRAMRange,0,__LINE__,0);
            mem = getVRAMRange();
            FB_END(getVRAMRange,0,__LINE__,0);
            if (mem)
            {
                const auto len = mem->getLength();
                fVramMapOffset
                    = base - mem->getPhysicalSegment(0, 0, kIOMemoryMapperNone);
                if (fVramMapOffset > len)
                {
                    assert(!(len & (len - 1))); // Power of 2 test
                    fVramMapOffset &= (len - 1);
                }
                setProperty( kIOFBMemorySizeKey, len, 32 );
                mem->release();
            }

            fVramMap = fbRange->map(kIOFBMapCacheMode);
            assert(fVramMap);
            if (fVramMap)
            {
                base = fVramMap->getVirtualAddress();
                fFrameBuffer = (volatile unsigned char *) base;

                IOG_KTRACE_NT(DBG_IOG_VRAM_CONFIG, DBG_FUNC_NONE,
                              __private->regID,
                              __private->pixelInfo.activeHeight,
                              __private->pixelInfo.bytesPerRow,
                              fVramMap->getLength());

                // Defensive fix for the panic aspect of:
                // <rdar://problem/20429613> SEED: BUG: kernel panic after
                //   connecting to a display in closed clamshell@doSetPowerState
                if (haveFB)
                {
                    // If the map length is below the current resolution,
                    // report the error
                    IOByteCount frameBufferLength = fVramMap->getLength();
                    if (frameBufferLength < apertureLen)
                    {
                        // If we end up here even after remap, then the vendor
                        // has a driver bug They are reporting a memory range
                        // that is less than required for the active size &
                        // depth.
                        IOLog("VENDOR_BUG: Invalid aperture range found "
                              "during setup for: %#llx (%#x:%u:%llu)\n",
                              __private->regID, __private->online, apertureLen,
                              frameBufferLength);
                    }
                }
            }

            DEBG1(thisName, " using (%dx%d,%d bpp)\n",
                  (uint32_t) __private->pixelInfo.activeWidth,
                  (uint32_t) __private->pixelInfo.activeHeight,
                  (uint32_t) __private->pixelInfo.bitsPerPixel );
            fbRange->release();
		}
	}

    if (full)
    {
        deliverDisplayModeDidChangeNotification();

        dpUpdateConnect();
    }
    
    DEBG1(thisName,
          " doSetup(%d) vram %d, fb %d\n", full, fVramMap != NULL, haveFB);
    if (haveFB)
    {
		if (__private->needsInit) initFB();
        setupCursor();
    }
    else
    {
        cursorBlitProc   = (CursorBlitProc)   NULL;
        cursorRemoveProc = (CursorRemoveProc) NULL;
        __private->framebufferWidth  = 0;
        __private->framebufferHeight = 0;
    }

    // reset console
    if (full && (haveFB || !gIOFBConsoleFramebuffer))
        findConsole();

    IOFB_END(doSetup,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

bool IOFramebuffer::suspend(bool now)
{
    IOFB_START(suspend,now,0,0);
    if (now == suspended)
    {
        IOFB_END(suspend,true,0,0);
        return (true);
    }

    if (now)
    {
		stopCursor();
		checkDeferredCLUTSet();
		if (this == gIOFBConsoleFramebuffer)
		{
			setPlatformConsole(0, kPEDisableScreen, DBG_IOG_SOURCE_SUSPEND);
			gIOFBConsoleFramebuffer = 0;
		}
		deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );
        suspended = true;
    }
    else
    {
		TIMESTART();
		setupForCurrentConfig();
		TIMEEND(thisName, "exit suspend setupForCurrentConfig time: %qd ms\n");

        suspended = false;
    }

    IOFB_END(suspend,false,0,0);
    return (false);
}

IOReturn IOFramebuffer::extSetDisplayMode(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extSetDisplayMode,0,0,0);
    IOFramebuffer * inst        = (IOFramebuffer *) target;
    IODisplayModeID displayMode = static_cast<IODisplayModeID>(args->scalarInput[0]);
    IOIndex         depth       = static_cast<IOIndex>(args->scalarInput[1]);
    IOReturn        err;

    GMETRICFUNC(DBG_IOG_SET_DISPLAY_MODE, kGMETRICS_EVENT_START,
               kGMETRICS_DOMAIN_FRAMEBUFFER | kGMETRICS_DOMAIN_DISPLAYMODE);
    IOG_KTRACE(DBG_IOG_SET_DISPLAY_MODE, DBG_FUNC_START,
               0, DBG_IOG_SOURCE_EXT_SET_DISPLAY_MODE,
               0, inst->__private->regID,
               0, displayMode,
               0, depth);
    err = inst->doSetDisplayMode(displayMode, depth);
    IOG_KTRACE(DBG_IOG_SET_DISPLAY_MODE, DBG_FUNC_END,
               0, DBG_IOG_SOURCE_EXT_SET_DISPLAY_MODE,
               0, inst->__private->regID,
               0, err, 0, 0);
    GMETRICFUNC(DBG_IOG_SET_DISPLAY_MODE, kGMETRICS_EVENT_END,
               kGMETRICS_DOMAIN_FRAMEBUFFER | kGMETRICS_DOMAIN_DISPLAYMODE);
    IOFB_END(extSetDisplayMode,err,0,0);
	return (err);
}

IOReturn IOFramebuffer::doSetDisplayMode(
		IODisplayModeID displayMode, IOIndex depth)
{
    IOFB_START(doSetDisplayMode,displayMode,depth,0);
    IOReturn        err;

    DEBG1(thisName, " extSetDisplayMode(0x%x, %d) susp %d online %d\n", 
    		(int32_t) displayMode, (uint32_t) depth, suspended,
            __private->online);

    if (kIODisplayModeIDAliasBase & displayMode)
    {
    	// && (depth == __private->currentDepth))

		if ((err = extEntry(false, kIOGReportAPIState_SetDisplayMode)))
        {
            IOFB_END(doSetDisplayMode,err,__LINE__,0);
			return (err);
        }

		__private->aliasMode = displayMode & ~kIODisplayModeIDAliasBase;
        DEBG(thisName, " nop set mode; set aliasMode 0x%08x\n", __private->aliasMode);
		extExit(err, kIOGReportAPIState_SetDisplayMode);

        IOFB_END(doSetDisplayMode,kIOReturnSuccess,__LINE__,0);
        return (kIOReturnSuccess);
    }

    if ((err = extEntrySys(false, kIOGReportAPIState_SetDisplayMode)))
    {
        IOFB_END(doSetDisplayMode,err,__LINE__,0);
        return (err);
    }

   	if (kIODisplayModeIDCurrent == displayMode)
	{
        FB_START(getCurrentDisplayMode,0,__LINE__,0);
        err = getCurrentDisplayMode(&displayMode, &depth);
        FB_END(getCurrentDisplayMode,err,__LINE__,0);
        IOG_KTRACE(DBG_IOG_GET_CURRENT_DISPLAY_MODE, DBG_FUNC_NONE,
                   0, DBG_IOG_SOURCE_DO_SET_DISPLAY_MODE,
                   0, __private->regID,
                   0, GPACKUINT32T(1, depth) | GPACKUINT32T(0, displayMode),
                   0, err);
	}

	suspend(true);

   	if (kIODisplayModeIDCurrent != displayMode)
	{
        const uint64_t metricFunc = DBG_IOG_SET_DISPLAY_MODE;
        const uint64_t metricDomain
            = kGMETRICS_DOMAIN_FRAMEBUFFER | kGMETRICS_DOMAIN_DISPLAYMODE
            | kGMETRICS_DOMAIN_VENDOR;

        sendLimitState(__LINE__);

		TIMESTART();
        FB_START(setDisplayMode,0,__LINE__,0);
        GMETRICFUNC(metricFunc, kGMETRICS_EVENT_START, metricDomain);
		err = setDisplayMode( displayMode, depth );
        GMETRICFUNC(metricFunc, kGMETRICS_EVENT_END, metricDomain);
        FB_END(setDisplayMode,err,__LINE__,0);
		TIMEEND(thisName, "setDisplayMode time: %qd ms\n");
        if (kIOReturnSuccess == err)
        {
            __private->lastSuccessfulMode = displayMode;
        }
		__private->aliasMode    = kIODisplayModeIDInvalid;
		__private->currentDepth = depth;
        DEBG(thisName, " invalidating aliasMode 0x%08x, currentDepth %d\n",
            __private->aliasMode, __private->currentDepth);
	}

    suspend(false);

	extExitSys(err, kIOGReportAPIState_SetDisplayMode);

    IOFB_END(doSetDisplayMode,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::checkMirrorSafe( UInt32 value, IOFramebuffer * other )
{
    IOFB_START(checkMirrorSafe,value,0,0);
    IOReturn        err = kIOReturnSuccess;
    IOFramebuffer * next = this;

    while ((next = next->getNextDependent()) && (next != this))
    {
		DEBG1(next->thisName, " transform 0x%llx \n", __private->transform);
        if (~kIOFBScalerUnderscan & (__private->transform ^ next->getTransform()))
        {
            err = kIOReturnUnsupported;
            break;
        }
    }

    IOFB_END(checkMirrorSafe,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::extSetMirrorOne(uint32_t value, IOFramebuffer * other)
{
    IOFB_START(extSetMirrorOne,value,0,0);
    IOReturn    err;
    IOFramebuffer * next;
    uintptr_t   data[2];
    bool        was;

	if (value && __private->nextMirror)
    {
        IOFB_END(extSetMirrorOne,kIOReturnBusy,0,0);
        return (kIOReturnBusy);
    }
	if (value && !other)
    {
        IOFB_END(extSetMirrorOne,kIOReturnBadArgument,0,0);
        return (kIOReturnBadArgument);
    }
	if (!value && !__private->nextMirror)
    {
        IOFB_END(extSetMirrorOne,kIOReturnSuccess,__LINE__,0);
        return (kIOReturnSuccess);
    }

    if (!value && __private->nextMirror)
	{
		next = this;
		do
		{
			next->suspend(true);
			data[0] = value;
			data[1] = (uintptr_t) next->__private->nextMirror;
			DEBG1(next->thisName, " kIOMirrorAttribute(0)\n"); 
            FB_START(setAttribute,kIOMirrorAttribute,__LINE__,0);
			err = next->setAttribute(kIOMirrorAttribute, (uintptr_t) &data);
            FB_END(setAttribute,err,__LINE__,0);
			DEBG1(next->thisName, " kIOMirrorAttribute(%d) ret 0x%x\n", value, err);
		}
		while ((next = next->__private->nextMirror) && (next != this));

		next = this;
		do
		{
			IOFramebuffer * prev;
			prev = next;
			next = next->__private->nextMirror;
			prev->__private->nextMirror = 0;
			prev->suspend(false);
		}
		while (next && (next != this));

        IOFB_END(extSetMirrorOne,kIOReturnSuccess,__LINE__,0);
		return (kIOReturnSuccess);
	}

	was = suspend(true);
	data[0] = value;
	data[1] = (uintptr_t) other;
    FB_START(setAttribute,kIOMirrorAttribute,__LINE__,0);
	err = setAttribute(kIOMirrorAttribute, (uintptr_t) &data);
    FB_END(setAttribute,err,__LINE__,0);
	DEBG1(thisName, " kIOMirrorAttribute(%d) ret 0x%x\n", value, err);
	if (kIOReturnSuccess != err)
	{
		if (!was) suspend(false);
	}
	else
	{
		__private->nextMirror = other;
		if (other->__private->nextMirror)
		{
			next = other;
			do next->suspend(false);
			while ((next = next->__private->nextMirror) && (next != other));
		}
	}
    IOFB_END(extSetMirrorOne,err,0,0);
	return (err);
}

IOReturn IOFramebuffer::setWSAAAttribute(IOSelect attribute, uint32_t value)
{
    IOFB_START(setWSAAAttribute,attribute,value,0);
    FBASSERTGATED(this);
    IOReturn    err = kIOReturnSuccess;
    const uint64_t metricFunc = DBG_IOG_SET_ATTRIBUTE_EXT;
    const uint64_t metricDomain
        = kGMETRICS_DOMAIN_FRAMEBUFFER | kGMETRICS_DOMAIN_VENDOR
        | kGMETRICS_DOMAIN_SLEEP       | kGMETRICS_DOMAIN_WAKE;
    const auto oldwsaa = __private->wsaaState; (void) oldwsaa;

    // <rdar://problem/24449391> J94 Fuji- color screen on external while rebooting with MST display attached
    /* Discussion:
     Sending the Will/Did change event before kIOWSAA_From_Accelerated forces the IOAcceleratorFamily
     to issue a flip from the scanout buffer back to the IOFramebuffer owned framebuffer, thus preventing the
     teardown of a scanout buffer that is in active use since the flip was deferred due to kIOWSAA_From_Accelerated.
     */
    switch (value & kIOWSAA_StateMask)
    {
        case kIOWSAA_To_Accelerated:
        case kIOWSAA_From_Accelerated:
        {
            if (kIOWSAA_Transactional & value)
            {
                __private->transactionsEnabled = true;
            }
            // Fallthru
        }
        case kIOWSAA_Sleep:
            /* case kIOWSAA_Hibernate: */
        {
            // Enter deferred state, then send the deferred event

            deliverFramebufferNotification(kIOFBNotifyWSAAWillEnterDefer, reinterpret_cast<void *>(static_cast<uintptr_t>(value & (~kIOWSAA_Reserved))));

            FB_START(setAttribute,attribute,__LINE__,value);
            GMETRICFUNC(metricFunc, kGMETRICS_EVENT_START, metricDomain);
            IOG_KTRACE(DBG_IOG_WSAA_DEFER_ENTER, DBG_FUNC_START,
                       0, __private->regID, 0, value, 0, 0, 0, 0);
            __private->lastWSAAStatus = err = setAttribute( attribute, value );
            const auto a3 =
                GPACKBIT(63, 1) |
                GPACKUINT32T(0, err);
            IOG_KTRACE(DBG_IOG_WSAA_DEFER_ENTER, DBG_FUNC_END,
                       0, __private->regID, 0, value, 0, a3, 0, 0);
            GMETRICFUNC(metricFunc, kGMETRICS_EVENT_END, metricDomain);
            FB_END(setAttribute,err,__LINE__,0);

            // Only save the state if the vendor responded favorably to the attribute.
            if (kIOReturnSuccess == err)
            {
                __private->wsaaState = value;
            }
            else
            {
                __private->transactionsEnabled = false;
            }

            deliverFramebufferNotification(kIOFBNotifyWSAADidEnterDefer, reinterpret_cast<void *>(static_cast<uintptr_t>(value & (~kIOWSAA_Reserved))));
            break;
        }
        case kIOWSAA_Unaccelerated:
        {
            __private->transactionsEnabled = false;
            // Fallthru
        }
        case kIOWSAA_Accelerated:
        {
            if (kIOWSAA_Transactional & value)
            {
                __private->transactionsEnabled = true;
            }

            // <rdar://problem/31336033> Intel Logout Shows Apple Logo followed by Black Screen with CD pax which removes CPU Blit
            // Send notifications first, then attribute for defer exit cases.
            deliverFramebufferNotification(kIOFBNotifyWSAAWillExitDefer, reinterpret_cast<void *>(static_cast<uintptr_t>(value & (~kIOWSAA_Reserved))));

            FB_START(setAttribute,attribute,__LINE__,value);
            GMETRICFUNC(metricFunc, kGMETRICS_EVENT_START, metricDomain);
            IOG_KTRACE(DBG_IOG_WSAA_DEFER_EXIT, DBG_FUNC_START,
                       0, __private->regID, 0, value, 0, 0, 0, 0);
            __private->lastWSAAStatus = err = setAttribute( attribute, value );
            const auto a3 =
                GPACKBIT(63, 1) |
                GPACKUINT32T(0, err);
            IOG_KTRACE(DBG_IOG_WSAA_DEFER_EXIT, DBG_FUNC_END,
                       0, __private->regID, 0, value, 0, a3, 0, 0);
            GMETRICFUNC(metricFunc, kGMETRICS_EVENT_END, metricDomain);
            FB_END(setAttribute,err,__LINE__,0);

            // Only save the state if the vendor responded favorably to the attribute.
            if (kIOReturnSuccess == err)
            {
                __private->wsaaState = value;
            }
            else
            {
                __private->transactionsEnabled = false;
            }

            deliverFramebufferNotification(kIOFBNotifyWSAADidExitDefer, reinterpret_cast<void *>(static_cast<uintptr_t>(value & (~kIOWSAA_Reserved))));

            break;
        }
            /* case kIOWSAA_DeferStart: */
            /* case kIOWSAA_DeferEnd: */
            /* case kIOWSAA_Transactional: */
            /* case kIOWSAA_Reserved: */
        case kIOWSAA_DriverOpen:
        default:
        {
            // These attributes require no action be sent to the vendor driver.
            break;
        }
    }

    DEBG1(thisName, "(0x%x) 0x%x was 0x%x ret %x\n",
          value, __private->wsaaState, oldwsaa, err);
    IOFB_END(setWSAAAttribute,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::extSetAttribute(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extSetAttribute,0,0,0);
    IOFramebuffer * inst      = (IOFramebuffer *) target;
    IOSelect        attribute = static_cast<IOSelect>(args->scalarInput[0]);
    uint32_t        value     = static_cast<uint32_t>(args->scalarInput[1]);
    IOFramebuffer * other     = (IOFramebuffer *) reference;
    bool            bAllow    = false;

    IOReturn    err;

    if (kIOWindowServerActiveAttribute == attribute)
        bAllow = true;

    if ((err = inst->extEntrySys(bAllow, kIOGReportAPIState_SetAttribute)))
    {
        IOFB_END(extSetAttribute,err,__LINE__,0);
        return (err);
    }
    
    switch (attribute)
    {
        case kIOMirrorAttribute:

            DEBG1(inst->thisName, " kIOMirrorAttribute(%d) susp(%d), curr(%d)\n", 
                    value, inst->suspended, (inst->__private->nextMirror != 0));
            value = (value != 0);
            if (value)
            {
                err = inst->checkMirrorSafe(value, other);
                if (kIOReturnSuccess != err)
                    break;
            }
			err = inst->extSetMirrorOne(value, other);
            break;

        case kIOCursorControlAttribute:
			err = kIOReturnBadArgument;
			break;

        case kIOWindowServerActiveAttribute:
            err = inst->setWSAAAttribute(attribute, value);
            break;
        case kIOFBRedGammaScaleAttribute:
            if (inst->setProperty(kIOFBRedGammaScale, value, 32))
            {
                err = kIOReturnSuccess;
            }
            else
            {
                err = kIOReturnNotWritable;
            }
            break;
        case kIOFBGreenGammaScaleAttribute:
            if (inst->setProperty(kIOFBGreenGammaScale, value, 32))
            {
                err = kIOReturnSuccess;
            }
            else
            {
                err = kIOReturnNotWritable;
            }
            break;
        case kIOFBBlueGammaScaleAttribute:
            if (inst->setProperty(kIOFBBlueGammaScale, value, 32))
            {
                err = kIOReturnSuccess;
            }
            else
            {
                err = kIOReturnNotWritable;
            }
            break;
        default:
            FB_START(setAttribute,attribute,__LINE__,value);
            err = inst->setAttribute( attribute, value );
            FB_END(setAttribute,err,__LINE__,0);
            break;
    }

	inst->extExitSys(err, kIOGReportAPIState_SetAttribute);

    IOFB_END(extSetAttribute,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::extGetAttribute(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extGetAttribute,0,0,0);
    IOFramebuffer * inst      = (IOFramebuffer *) target;
    IOSelect        attribute = static_cast<IOSelect>(args->scalarInput[0]);
    uint64_t *      value     = &args->scalarOutput[0];
    IOFramebuffer * other     = (IOFramebuffer *) reference;
    IOReturn        err       = kIOReturnSuccess;

    *value = 0;


    switch (attribute)
    {
        case kIOFBProcessConnectChangeAttribute:
            err = inst->extProcessConnectionChange();
            break;

        case kIOFBEndConnectChangeAttribute:
            if ((err = inst->extEntrySys(true, kIOGReportAPIState_EndConnectionChange)))
            {
                IOFB_END(extGetAttribute,err,__LINE__,0);
                return (err);
            }

            err = inst->extEndConnectionChange();
            inst->extExitSys(err, kIOGReportAPIState_EndConnectionChange);
            break;

        case kIOFBWSStartAttribute:
            if ((err = inst->extEntrySys(true, kIOGReportAPIState_WSStartAttribute)))
            {
                IOFB_END(extGetAttribute,err,__LINE__,0);
                return (err);
            }

            if (inst->__private->controller->fWsWait)
            {
                DEBG1(inst->thisName, " kIOFBWSStartAttribute fWsWait %d\n", inst->__private->controller->fWsWait);
                inst->__private->controller->fWsWait &= ~(1 << inst->__private->controllerIndex);
                if (!inst->__private->controller->fWsWait)
                {
                    IOFramebuffer * fb0 = inst->__private->controller->fFbs[0];
                    DEBG1(inst->thisName, " fWsWait done remsg %d\n", fb0->messaged);
                    if (fb0->messaged)
                    {
                        fb0->messageClients(kIOMessageServiceIsSuspended, (void *) true);
                    }
                    resetClamshell(kIOFBClamshellProbeDelayMS,
                                   DBG_IOG_SOURCE_EXT_GET_ATTRIBUTE);
                    inst->__private->controller->startThread();
                }
            }
            inst->extExitSys(err, kIOGReportAPIState_WSStartAttribute);
            break;

        case kIOFBRedGammaScaleAttribute:
        {
            OSNumber   *  num = OSDynamicCast(OSNumber, inst->getProperty(kIOFBRedGammaScale));
            if (num)
            {
                value[0] = num->unsigned32BitValue();
                err = kIOReturnSuccess;
            }
            else
            {
                err = kIOReturnNotReadable;
            }
            break;
        }
        case kIOFBGreenGammaScaleAttribute:
        {
            OSNumber   *  num = OSDynamicCast(OSNumber, inst->getProperty(kIOFBGreenGammaScale));
            if (num)
            {
                value[0] = num->unsigned32BitValue();
                err = kIOReturnSuccess;
            }
            else
            {
                err = kIOReturnNotReadable;
            }
            break;
        }
        case kIOFBBlueGammaScaleAttribute:
        {
            OSNumber   *  num = OSDynamicCast(OSNumber, inst->getProperty(kIOFBBlueGammaScale));
            if (num)
            {
                value[0] = num->unsigned32BitValue();
                err = kIOReturnSuccess;
            }
            else
            {
                err = kIOReturnNotReadable;
            }
            break;
        }
        case kConnectionStartOfFrameTime:
        {
            uintptr_t result = 0;

            FB_START(getAttribute,attribute,__LINE__,0);
            err = inst->getAttribute( attribute, &result );
            FB_END(getAttribute,err,__LINE__,0);
            if (kIOReturnSuccess == err) *value = result;

            break;
        }
        default:

            if ((err = inst->extEntry(false, kIOGReportAPIState_GetAttribute)))
            {
                IOFB_END(extGetAttribute,err,__LINE__,0);
                return (err);
            }

            bool overridden = false;

            if (!overridden) {
                uintptr_t result = (uintptr_t) other;
                FB_START(getAttribute,attribute,__LINE__,0);
                err = inst->getAttribute( attribute, &result );
                FB_END(getAttribute,err,__LINE__,0);
                if (kIOReturnSuccess == err) *value = (UInt32) result;
            } // if (!overridden)


            inst->extExit(err, kIOGReportAPIState_GetAttribute);
            break;
    }

    IOFB_END(extGetAttribute,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::extGetInformationForDisplayMode(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFB_START(extGetInformationForDisplayMode,0,0,0);
    IOFramebuffer * inst   = (IOFramebuffer *) target;
    IODisplayModeID mode   = static_cast<IODisplayModeID>(args->scalarInput[0]);
    void *          info   = args->structureOutput;
    IOByteCount     length = args->structureOutputSize;

    UInt32                       flags = 0;
    IOReturn                     err;
    bool                         getTiming;
    IOFBDisplayModeDescription * out = (IOFBDisplayModeDescription *) info;

    if (length < sizeof(IODisplayModeInformation))
    {
        IOFB_END(extGetInformationForDisplayMode,kIOReturnBadArgument,0,0);
        return (kIOReturnBadArgument);
    }

    if ((err = inst->extEntry(false, kIOGReportAPIState_GetInformationForDisplayMode)))
    {
        IOFB_END(extGetInformationForDisplayMode,err,__LINE__,0);
        return (err);
    }

    FB_START(getInformationForDisplayMode,0,__LINE__,0);
    err = inst->getInformationForDisplayMode( mode, &out->info );
    FB_END(getInformationForDisplayMode,err,__LINE__,0);
    if (kIOReturnSuccess == err)
    {
        err = IODisplayWrangler::getFlagsForDisplayMode( inst, mode, &flags);
        if (kIOReturnSuccess == err)
        {
            out->info.flags &= ~kDisplayModeSafetyFlags;
            out->info.flags |= flags;
        }
        getTiming = (length >= sizeof(IOFBDisplayModeDescription));
        out->timingInfo.flags = getTiming ? kIODetailedTimingValid : 0;
        FB_START(getTimingInfoForDisplayMode,mode,__LINE__,0);
        IOReturn kr = inst->getTimingInfoForDisplayMode(mode, &out->timingInfo);
        FB_END(getTimingInfoForDisplayMode,kr,__LINE__,0);
        if (kIOReturnSuccess != kr)
        {
            out->timingInfo.flags &= ~kIODetailedTimingValid;
            out->timingInfo.appleTimingID = 0;
        }
    }

	inst->extExit(err, kIOGReportAPIState_GetInformationForDisplayMode);

    IOFB_END(extGetInformationForDisplayMode,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::setDisplayAttributes(OSObject * obj)
{
    IOFB_START(setDisplayAttributes,0,0,0);
    IOReturn       r, ret = kIOReturnSuccess;
    uint32_t *     attributes;
    uint32_t       idx, max;
    uint32_t       attr, attrValue, value, mask;
    uint32_t       controllerDepths, ditherMask = 0;
    uintptr_t      lvalue[16];
    bool           found = false;
    bool           skip = false;
    bool           updatesMode = false;

    if (!obj)
    {
        IOFB_END(setDisplayAttributes,kIOReturnSuccess,__LINE__,0);
        return (kIOReturnSuccess);
    }

    obj->retain();
    OSSafeReleaseNULL(__private->displayAttributes);
    __private->displayAttributes = obj;

    if (__private->display)
        __private->display->setProperty(kIODisplayAttributesKey, obj);

    OSDictionary* dict = OSDynamicCast(OSDictionary, obj);
    OSData* data = OSDynamicCast(OSData,
            (dict ? dict->getObject(kIODisplayAttributesKey) : obj));
    if (!data)
    {
        IOFB_END(setDisplayAttributes,kIOReturnSuccess,__LINE__,0);
        return (kIOReturnSuccess);
    }

    attributes = (uint32_t *) data->getBytesNoCopy();
    max        = data->getLength() / sizeof(attributes[0]);

    for (idx = 0; idx < max; idx += 2)
    {
        attr      = attributes[idx];
        attrValue = attributes[idx + 1];

        if (kConnectionVendorTag == attr)
        {
            if (found)
                break;
            skip = (attrValue && (attrValue != __private->controller->fVendorID));
            found = !skip;
            continue;
        }
        if (skip)
            continue;
        
        DEBG1(thisName, " (0x%08x '%c%c%c%c')(%x)\n", attr, FEAT(attr), attrValue);
        
        updatesMode = false;
        switch (attr)
        {
            case kConnectionColorModesSupported:
                attrValue |= kIODisplayColorModeAuto;
                attrValue |= kIODisplayColorModeRGBLimited;
                __private->colorModesSupported = attrValue;
                break;

            case kConnectionColorDepthsSupported:

                FB_START(getAttributeForConnection,kConnectionControllerDepthsSupported,__LINE__,0);
                r = getAttributeForConnection(0, kConnectionControllerDepthsSupported, &lvalue[0]);
                FB_END(getAttributeForConnection,r,__LINE__,0);
                if (kIOReturnSuccess != r)
                {
                    lvalue[0] =  kIODisplayRGBColorComponentBits6
                    | kIODisplayRGBColorComponentBits8;
                    if (10 == __private->desiredGammaDataWidth)
                        lvalue[0] |= kIODisplayRGBColorComponentBits10;
                }
                
                controllerDepths = static_cast<uint32_t>(lvalue[0]);
                
#define COLRMASK(n)   ((n ## 16) | (n ## 14) | (n ## 12) | (n ## 10) | (n ## 8) | (n ## 6))
#define HIGHBIT(v)    (1 << (31 - __builtin_clz(v)))

                value = controllerDepths & attrValue;
                if (!value)
                    value = kIODisplayRGBColorComponentBits8;

                mask = COLRMASK(kIODisplayRGBColorComponentBits);
                if (value & mask)
                {
                    value = (value & ~mask) | HIGHBIT(value & mask);
                    if (!(value & HIGHBIT(controllerDepths & mask)))
                        ditherMask |= (kIODisplayDitherAll << kIODisplayDitherRGBShift);
                }

                mask = COLRMASK(kIODisplayYCbCr444ColorComponentBits);
                if (value & mask)
                {
                    value = (value & ~mask) | HIGHBIT(value & mask);
                    if (!(value & HIGHBIT(controllerDepths & mask)))
                        ditherMask |= (kIODisplayDitherAll << kIODisplayDitherYCbCr444Shift);
                }

                mask = COLRMASK(kIODisplayYCbCr422ColorComponentBits);
                if (value & mask)
                {
                    value = (value & ~mask) | HIGHBIT(value & mask);
                    if (!(value & HIGHBIT(controllerDepths & mask)))
                        ditherMask |= (kIODisplayDitherAll << kIODisplayDitherYCbCr422Shift);
                }

                // pass thru kConnectionColorDepthsSupported
                DEBG1(thisName, " (0x%08x '%c%c%c%c')(%x)\n", attr, FEAT(attr), attrValue);
                FB_START(setAttributeForConnection,attr,__LINE__,attrValue);
                r = setAttributeForConnection(0, attr, attrValue);
                FB_END(setAttributeForConnection,r,__LINE__,0);
                if (kIOReturnSuccess != r)
                    ret = r;

                attr = kConnectionControllerColorDepth;
                attrValue = value;
                updatesMode = true;
                break;
                
            case kConnectionControllerDitherControl:
                
                attrValue &= ditherMask;
                updatesMode = true;
                break;
        }
        
        DEBG1(thisName, " (0x%08x '%c%c%c%c')(%x)\n", attr, FEAT(attr), attrValue);
        
        if (updatesMode)
        {
            FB_START(getAttributeForConnection,attr,__LINE__,0);
            r = getAttributeForConnection(0, attr, &lvalue[0]);
            FB_END(getAttributeForConnection,r,__LINE__,0);
            if (kIOReturnSuccess != r)
                continue;
            if (lvalue[0] == attrValue)
                continue;
        }
        FB_START(setAttributeForConnection,attr,__LINE__,attrValue);
        ret = setAttributeForConnection(0, attr, attrValue);
        FB_END(setAttributeForConnection,r,__LINE__,0);
    }
    
    IOFB_END(setDisplayAttributes,ret,0,0);
    return (ret);
}

IOReturn IOFramebuffer::extSetProperties(OSDictionary* props)
{
    IOFB_START(extSetProperties,0,0,0);

    IOReturn err =
        extEntry(/* allowOffline */ true, kIOGReportAPIState_SetProperties);
    if (err) {
        IOFB_END(extSetProperties,err,__LINE__,0);
        return err;
    }

    bool validProperty = false;

    // Decode IOFBConfig
    OSObject* obj = props->getObject(gIOFBConfigKey);
    OSDictionary* dict = OSDynamicCast(OSDictionary, obj);
    if (static_cast<bool>(dict)) {
        validProperty = true;
        setProperty(gIOFBConfigKey, dict);
        if (__private->online) {
            if (dict->getObject("IOFBScalerUnderscan"))
                __private->enableScalerUnderscan = true;
            auto *array = OSDynamicCast(
                    OSArray, dict->getObject(kIOFBDetailedTimingsKey));
            if (array)
                err = doSetDetailedTimings(
                        array, DBG_IOG_SOURCE_EXT_SET_PROPERTIES, __LINE__);
        }
    } else if (static_cast<bool>(obj))
        IOLog("[%s] Error: bad format ignoring %s\n",
                thisName, gIOFBConfigKey->getCStringNoCopy());

    // Decode IODisplayAttributes
    obj = props->getObject(kIODisplayAttributesKey);
    OSDictionary* attrDict  = OSDynamicCast(OSDictionary, obj);
    if (static_cast<bool>(attrDict)) {
        // With the IOBacklight module we get IODisplayAttributes from two
        // sources, CoreDisplay and IOBacklight itself.  Just replacing the
        // displayAttributes would cause data to be lost.  Since
        // IODictionary::merge replaces current entries with the entries from a
        // source we merge in new values, as we locked here this is safe.  Also
        // setDisplayAttributes with the current values is called on connect
        // change so it is incumbent to save old untouched values.
        validProperty = true;
        dict = nullptr;
        if (static_cast<bool>(__private->displayAttributes)) {
            dict = static_cast<OSDictionary*>(__private->displayAttributes);
            dict = OSDictionary::withDictionary(dict);
            dict->merge(attrDict);
            attrDict = dict;
        }
        const IOReturn setDispErr = setDisplayAttributes(attrDict);
        OSSafeReleaseNULL(dict);
        if (!err)
            err = setDispErr;
    } else if (static_cast<bool>(obj))
        IOLog("[%s] Error: bad format ignoring %s\n",
                thisName, kIODisplayAttributesKey);

    if (!err && !validProperty)
        err = kIOReturnUnsupported;

    extExit(err, kIOGReportAPIState_SetProperties);

    IOFB_END(extSetProperties,err,0,0);
    return (err);
}




//// Controller attributes

IOReturn IOFramebuffer::setAttribute( IOSelect attribute, uintptr_t value )
{
    IOFB_START(setAttribute,attribute,value,0);
    IOReturn        ret;

    switch (attribute)
    {
        case kIOCapturedAttribute:
        {
            DEBG(thisName, " kIOCapturedAttribute(%ld)\n", value);
			if (value != gIOFBCaptureState)
			{
				gIOFBCaptureState = static_cast<uint32_t>(value);
                atomic_fetch_or_explicit(&gIOFBGlobalEvents,
                        kIOFBEventCaptureSetting, memory_order_relaxed);
				__private->controller->didWork(kWorkStateChange);
			}
            ret = kIOReturnSuccess;
            break;
        }
        
        case kIOCursorControlAttribute:
            {
                IOFBCursorControlAttribute * crsrControl;

                crsrControl = (IOFBCursorControlAttribute *) value;

                if (__private->cursorThread)
                {
                    __private->cursorThread->release();
                    __private->cursorThread = 0;
                }

                if (crsrControl && crsrControl->callouts)
                {
                    __private->cursorControl = *((IOFBCursorControlAttribute *) value);
                    __private->cursorThread = IOInterruptEventSource::interruptEventSource(this, &cursorWork);
                    IOWorkLoop * const workloop = getWorkLoop();
                    if (workloop && __private->cursorThread)
                        workloop->addEventSource(__private->cursorThread);
                }
                ret = kIOReturnSuccess;
                break;
            }

		case kIOPowerStateAttribute: {
            const uint64_t metricFunc = DBG_IOG_SET_ATTRIBUTE_EXT;
            const uint64_t metricDomain
                = kGMETRICS_DOMAIN_VENDOR | kGMETRICS_DOMAIN_POWER
                | GMETRIC_DOMAIN_FROM_POWER_STATE(value, getPowerState());
            FB_START(setAttribute,kIOPowerAttribute,__LINE__,value);
            GMETRICFUNC(metricFunc, kGMETRICS_EVENT_START, metricDomain);
            ret = setAttribute(kIOPowerAttribute, value);
            GMETRICFUNC(metricFunc, kGMETRICS_EVENT_END, metricDomain);
            FB_END(setAttribute,ret,__LINE__,0);
            break;
        }
            
        case kIOPowerAttribute:
            ret = setAttributeExt(attribute, value);
            break;

        case kIOFBLimitHDCPStateAttribute:
            setLimitState(static_cast<uint64_t>(value));
            ret = kIOReturnSuccess;
            break;

        case kIOBuiltinPanelPowerAttribute: {
            if (!__private || !__private->controller) {
                ret = kIOReturnInternalError;
                break;
            }
            const auto a1 =
                GPACKUINT8T(0, static_cast<bool>(gIOGraphicsControl)) |
                GPACKUINT8T(1, __private->controller->isMuted()) |
                GPACKUINT8T(2, value);
            IOG_KTRACE_NT(DBG_IOG_BUILTIN_PANEL_POWER, DBG_FUNC_NONE,
                __private->regID, a1, 0, 0);
            if (!gIOGraphicsControl || !__private->controller->isMuted())
            {
                IODisplayWrangler::builtinPanelPowerNotify(
                    static_cast<bool>(value));
            }
            ret = kIOReturnSuccess;
            break;
        }

        default:
            ret = kIOReturnUnsupported;
            break;
    }

    IOFB_END(setAttribute,ret,0,0);
    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOFramebuffer::readClamshellState(uint64_t where)
{
    IOFB_START(readClamshellState,0,0,0);
    // zero -> lid open
    const uint32_t oldState = gIOFBLastClamshellState;

    static IOACPIPlatformDevice * lidDevice;
    UInt32 lidState;

    if (!lidDevice)
    {
        OSIterator *   iter;
        IOService *    service;
        OSDictionary * matching;

		matching = IOService::nameMatching("PNP0C0D");
        iter = IOService::getMatchingServices(matching);
        if (matching) matching->release();
        if (iter)
        {                       
            service = (IOService *)iter->getNextObject();
            if (service->metaCast("IOACPIPlatformDevice"))
            {
                lidDevice = (IOACPIPlatformDevice *) service;
                lidDevice->retain();
            }
            iter->release();
        }
    }

    if (lidDevice)
    {
    	IOReturn ret;
        ret = lidDevice->evaluateInteger("_LID", &lidState);
        if (kIOReturnSuccess == ret)
            gIOFBLastClamshellState = (lidState == 0);
    }

    DEBG1("S", " %d\n", (int) gIOFBLastClamshellState);
    IOG_KTRACE(DBG_IOG_CLAMSHELL, DBG_FUNC_NONE,
               0, DBG_IOG_SOURCE_READ_CLAMSHELL,
               0, where, 0, gIOFBLastClamshellState, 0, oldState);
    IOFB_END(readClamshellState,0,0,0);
}

namespace {
IOReturn getHardwareClamshellState(IOOptionBits *resultP)
{
    // zero -> lid open
	if (gIOFBDesktopModeAllowed)
		*resultP = gIOFBLastClamshellState;
	else
		*resultP = 0;
//	gIOFBLastReadClamshellState = gIOFBLastClamshellState;

    DEBG1("S", " %d\n", (int) *resultP);
    return kIOReturnSuccess;
}
}; // namespace

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFramebuffer::clamshellHandler(void * target, void * ref,
                                       IOService * resourceService, IONotifier * notifier)
{
    IOFB_START(clamshellHandler,0,0,0);

    if (!(atomic_load(&gIOGDebugFlags) & kIOGDbgClamshellInjectionEnabled))
    {
        uint64_t    clamshellState = DBG_IOG_CLAMSHELL_STATE_NOT_PRESENT;

        gIOResourcesAppleClamshellState = resourceService->getProperty(kAppleClamshellStateKey);
        if (gIOResourcesAppleClamshellState)
        {
            clamshellState = (kOSBooleanTrue == gIOResourcesAppleClamshellState)
            ? DBG_IOG_CLAMSHELL_STATE_CLOSED : DBG_IOG_CLAMSHELL_STATE_OPEN;
        }
        IOG_KTRACE(DBG_IOG_CLAMSHELL, DBG_FUNC_NONE,
                   0, DBG_IOG_SOURCE_CLAMSHELL_HANDLER,
                   0, clamshellState, 0, 0, 0, 0);

        resourceService->removeProperty(kAppleClamshellStateKey);
        triggerEvent(kIOFBEventReadClamshell);
    }

    IOFB_END(clamshellHandler,true,0,0);
	return (true);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::getAttribute(IOSelect attribute, uintptr_t *resultP)
{
    IOFB_START(getAttribute,attribute,0,0);
    IOReturn err = kIOReturnUnsupported;

    switch (attribute)
    {
        case kIOClamshellStateAttribute:
        {
            IOOptionBits result = 0; // lid open
            // Thunderbolt GPUs never control the built-in panel. Tell them
            // clamshell is always open.
            if (!__private->controller || !__private->controller->fExternal)
                err = getHardwareClamshellState(&result);
            *resultP = result;
            IOG_KTRACE(DBG_IOG_CLAMSHELL, DBG_FUNC_NONE,
                       0, DBG_IOG_SOURCE_GET_ATTRIBUTE,
                       0, __private->regID, 0, err, 0, *resultP);
            break;
        }

        case kIOFBLimitHDCPStateAttribute:
            if (resultP)
                *resultP = static_cast<uintptr_t>(getLimitState());
            break;

        default:
            break;
    }
    
    IOFB_END(getAttribute,err,0,0);
    return err;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFramebuffer::setNumber( OSDictionary * dict, const char * key,
                               UInt32 value )
{
    IOFB_START(setNumber,value,0,0);
    OSNumber *  num;
    bool        ok;

    num = OSNumber::withNumber( value, 32 );
    if (!num)
    {
        IOFB_END(setNumber,false,__LINE__,0);
        return (false);
    }

    ok = dict->setObject( key, num );
    num->release();

    IOFB_END(setNumber,ok,0,0);
    return (ok);
}

bool IOFramebuffer::serializeInfo( OSSerialize * s )
{
    IOFB_START(serializeInfo,0,0,0);
    IOReturn                    err;
    IODisplayModeInformation    info;
    IOPixelInformation          pixelInfo;
    IODisplayModeID *           modeIDs;
    IOItemCount                 modeCount, modeNum, aperture;
    IOIndex                     depthNum;
    OSDictionary *              infoDict;
    OSDictionary *              modeDict;
    OSDictionary *              pixelDict;
    char                        keyBuf[12];
    bool                        ok = true;

    FB_START(getDisplayModeCount,0,__LINE__,0);
    modeCount = getDisplayModeCount();
    FB_END(getDisplayModeCount,0,__LINE__,0);
    modeIDs = IONew( IODisplayModeID, modeCount );
    if (!modeIDs)
    {
        IOFB_END(serializeInfo,false,__LINE__,0);
        return (false);
    }

    FB_START(getDisplayModes,0,__LINE__,0);
    err = getDisplayModes( modeIDs );
    FB_END(getDisplayModes,err,__LINE__,0);
    if (err)
    {
        IOFB_END(serializeInfo,false,__LINE__,0);
        return (false);
    }

    infoDict = OSDictionary::withCapacity( 10 );
    if (!infoDict)
    {
        IOFB_END(serializeInfo,false,__LINE__,0);
        return (false);
    }

    for (modeNum = 0; modeNum < modeCount; modeNum++)
    {
        FB_START(getInformationForDisplayMode,0,__LINE__,0);
        err = getInformationForDisplayMode( modeIDs[modeNum], &info );
        FB_END(getInformationForDisplayMode,err,__LINE__,0);
        if (err)
            continue;

        modeDict = OSDictionary::withCapacity( 10 );
        if (!modeDict)
            break;

        ok = setNumber( modeDict, kIOFBWidthKey,
                        info.nominalWidth )
             && setNumber( modeDict, kIOFBHeightKey,
                           info.nominalHeight )
             && setNumber( modeDict, kIOFBRefreshRateKey,
                           info.refreshRate )
             && setNumber( modeDict, kIOFBFlagsKey,
                           info.flags );
        if (!ok)
            break;

        for (depthNum = 0; depthNum < info.maxDepthIndex; depthNum++)
        {
            for (aperture = 0; ; aperture++)
            {
                FB_START(getPixelInformation,0,__LINE__,0);
                err = getPixelInformation( modeIDs[modeNum], depthNum,
                                           aperture, &pixelInfo );
                FB_END(getPixelInformation,err,__LINE__,0);
                if (err)
                    break;

                pixelDict = OSDictionary::withCapacity( 10 );
                if (!pixelDict)
                    continue;

                ok = setNumber( pixelDict, kIOFBBytesPerRowKey,
                                pixelInfo.bytesPerRow )
                     && setNumber( pixelDict, kIOFBBytesPerPlaneKey,
                                   pixelInfo.bytesPerPlane )
                     && setNumber( pixelDict, kIOFBBitsPerPixelKey,
                                   pixelInfo.bitsPerPixel )
                     && setNumber( pixelDict, kIOFBComponentCountKey,
                                   pixelInfo.componentCount )
                     && setNumber( pixelDict, kIOFBBitsPerComponentKey,
                                   pixelInfo.bitsPerComponent )
                     && setNumber( pixelDict, kIOFBFlagsKey,
                                   pixelInfo.flags )
                     && setNumber( pixelDict, kIOFBWidthKey,
                                   pixelInfo.activeWidth )
                     && setNumber( pixelDict, kIOFBHeightKey,
                                   pixelInfo.activeHeight );
                if (!ok)
                    break;

                snprintf(keyBuf, sizeof(keyBuf), "%x", (int) (depthNum + (aperture << 16)));
                modeDict->setObject( keyBuf, pixelDict );
                pixelDict->release();
            }
        }

        snprintf(keyBuf, sizeof(keyBuf), "%x", (int) modeIDs[modeNum]);
        infoDict->setObject( keyBuf, modeDict );
        modeDict->release();
    }

    IODelete( modeIDs, IODisplayModeID, modeCount );

    ok &= infoDict->serialize( s );
    infoDict->release();

    IOFB_END(serializeInfo,ok,0,0);
    return (ok);
}

void IOFramebuffer::setLimitState(const uint64_t limit)
{
    __private->hdcpLimitState = limit;
}

uint64_t IOFramebuffer::getLimitState(void) const
{
    return(__private->hdcpLimitState);
}

void IOFramebuffer::resetLimitState(void)
{
    setLimitState(static_cast<uint64_t>(kIOFBHDCPLimit_NoHDCP20Type1));
}

void IOFramebuffer::sendLimitState(const uint32_t line)
{
    FB_START(setAttribute,kIOFBLimitHDCPAttribute,line,__private->hdcpLimitState);
    setAttribute(kIOFBLimitHDCPAttribute, __private->hdcpLimitState);
    FB_END(setAttribute,0,line,0);
}

#pragma mark -
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(_IOFramebufferNotifier, IONotifier)
#define LOCKNOTIFY()
#define UNLOCKNOTIFY()

bool _IOFramebufferNotifier::init(IOFramebufferNotificationHandler handler, OSObject * target, void * ref,
                                  IOIndex groupPriority, IOSelect events, int32_t groupIndex)
{
    bool    bRet = IONotifier::init();
    if (bRet)
    {
        // Record the notifier state.
        fEnable = false;
        fHandler = handler;
        fTarget = target;
        fRef = ref;
        if (groupPriority > kIOFBNotifyPriority_Max)
        {
            groupPriority = kIOFBNotifyPriority_Max;
        }
        else if (groupPriority < kIOFBNotifyPriority_Min)
        {
            groupPriority = kIOFBNotifyPriority_Min;
        }
        fGroupPriority = groupPriority;
        fEvents = events;
        fGroup = groupIndex;
    }
    
    return (bRet);
}

void _IOFramebufferNotifier::remove()
{
    DEBG("N", " Remove: %p\n", this);

    LOCKNOTIFY();

    fEnable = false;

    if (fWhence)
    {
        fWhence->removeObject( (OSObject *) this );
        fWhence = NULL;
    }

    UNLOCKNOTIFY();

    release();
}

bool _IOFramebufferNotifier::disable()
{
    bool        ret;

    DEBG("N", " Disable: %p\n", this);

    LOCKNOTIFY();
    ret = fEnable;
    fEnable = false;
    UNLOCKNOTIFY();

    return (ret);
}

void _IOFramebufferNotifier::enable( bool was )
{
    DEBG("N", " Enable: %p\n", this);

    LOCKNOTIFY();
    fEnable = was;
    UNLOCKNOTIFY();
}

#pragma mark - Framebuffer Notification -
IONotifier * IOFramebuffer::addFramebufferNotification(
    IOFramebufferNotificationHandler handler,
    OSObject * target, void * ref)
{
    IOFB_START(addFramebufferNotification,0,0,0);
    // Pass the legacy registration with "legacy" state
    IONotifier * notify = addFramebufferNotificationWithOptions(handler, target, ref, kIOFBNotifyGroupID_Legacy, 0, kIOFBNotifyEvent_All);
    IOFB_END(addFramebufferNotification,0,0,0);
    return (notify);
}

IONotifier * IOFramebuffer::addFramebufferNotificationWithOptions(IOFramebufferNotificationHandler handler,
                                                                  OSObject * target, void * ref,
                                                                  IOSelect groupID, IOIndex groupPriority, IOSelect events)
{
    IOFB_START(addFramebufferNotificationWithOptions,groupID,groupPriority,events);
    _IOFramebufferNotifier  * notify = NULL;
    OSOrderedSet            * notifySet = NULL;
    int32_t                 groupIndex = -1;

    // Make sure the events are valid - no point tracking a notification that has no events (and will never be executed)
    events &= kIOFBNotifyEvent_All;
    if (kIOFBNotifyEvent_None != events)
    {
#if 0
        /* To maintain legacy behavior with regards to the Will/DidNotify notifications
         force all registrations to want kIOFBNotifyEvent_Notify event notification.
         Note that the old callout order was:
         1 - Send Will event to all clients
         2 - Send event to all clients
         3 - Send Did event to all clients
         .. repeat the above for all clients
         All clients get all notification events.
         The new callout order is:
         1 - Send Will/event/Did to first client (Will/event/Did only sent if requested)
         2 - Send Will/event/Did to next client (Will/event/Did only sent if requested)
         3 - ...
         */
        events |= kIOFBNotifyEvent_Notify;
#endif
        // Decode/validate group id
        groupIndex = groupIDToIndex(groupID);
        if (static_cast<uint32_t>(groupIndex) <= kIOFBNotifyGroupIndex_LastIndex)
        {
            // Allocate a new notifier.
            notify = new _IOFramebufferNotifier;
            if (NULL != notify)
            {
                do
                {
                    if (!notify->init(handler, target, ref, groupPriority, events, groupIndex))
                    {
                        DEBG1(thisName, " Failed to initialize notifier for index %d\n", groupIndex);
                        break;
                    }

                    // Protect ourselves while we alter the collections.
                    const bool hasController = (__private && __private->controller);

                    IOGraphicsWorkLoop * const wl
                        = hasController ? FBWL(this) : gIOFBSystemWorkLoop;
                    const char * const name = hasController ? thisName : "S";
                    IOGraphicsWorkLoop::GateGuard gated(wl, name, __FUNCTION__);

                    // Allocate the master notification array first.
                    if (false == initNotifiers())
                    {
                        // Failed to create notifier tracking objects, bail.
                        break;
                    }

                    // Get the set from the array, if no set was created, create one and add it.
                    notifySet = (OSOrderedSet *)fFBNotifications->getObject(groupIndex);
                    if (NULL == notifySet)
                    {
                        // If no set found, then the notification tracking is messed up, bail.
                        IOLog("[%s] Error: no set found at index %d\n", thisName, groupIndex);
                        break;
                    }

                    if (true != notifySet->setObject( notify ))
                    {
                        IOLog( "[%s] Error: failed to set notify for index %d\n", thisName, groupIndex);
                        break;
                    }
                    else if (NULL != target)
                    {
                        strncpy(notify->fName, (target->getMetaClass())->getClassName(), sizeof(notify->fName) - 1);
                    }

                    DEBG1( thisName, " added notification: %p (%s) (%#x, %d, %#x) to group %d(%u)\n",
                          notify, notify->fName,
                          notify->fGroup, notify->fGroupPriority, notify->fEvents,
                          groupIndex, notifySet->getCount());

                    // Lastly, record notifier and enable
                    notify->fWhence = notifySet;
                    notify->fEnable = true;

                    // Success
                    return (notify);

                } while(0);

                // Error out case
                OSSafeReleaseNULL(notify);
            }
            else
            {
                DEBG1(thisName, " Failed to allocate notifier for index %d\n", groupIndex);
            }
        }
        else
        {
            DEBG1(thisName, " Invalid group ID: %#x\n", groupID);
        }
    }
    else
    {
        DEBG1(thisName, " Invalid events: %#x\n", events);
    }
    
    IOFB_END(addFramebufferNotificationWithOptions,0,0,0);
    return (notify);
}

IOReturn IOFramebuffer::deliverFramebufferNotification( IOIndex event, void * info )
{
    IOFB_START(addFramebufferNotification,event,0,0);
    IOReturn    ret = kIOReturnSuccess;
    IOSelect    eventMask = 0;
    IOSelect    groupIndex = 0;

    LOCKNOTIFY();
    __private->fNotificationActive = 1;
    __private->fNotificationGroup = 0;

#if RLOG1
    const auto startTime = mach_absolute_time();
#endif  /* RLOG1 */

    eventMask = eventToMask(event);

    // Determine callout order
    switch (event)
    {
        // Forward
        case kIOFBNotifyDisplayModeWillChange:
        case kIOFBNotifyWillSleep:
        case kIOFBNotifyDidPowerOff:
        case kIOFBNotifyWillPowerOff:
        case kIOFBNotifyWillChangeSpeed:
        case kIOFBNotifyWSAAWillEnterDefer:
        case kIOFBNotifyWSAADidEnterDefer:
        case kIOFBNotifyWillNotify:
        case kIOFBNotifyDisplayDimsChange:
        case kIOFBNotifyClamshellChange:
        case kIOFBNotifyCaptureChange:
        case kIOFBNotifyOnlineChange:
        case kIOFBNotifyProbed:
        case kIOFBNotifyVRAMReady:
        case kIOFBNotifyTerminated:
        case kIOFBNotifyHDACodecWillPowerOff:
        case kIOFBNotifyHDACodecDidPowerOff:
        {
            for (groupIndex = 0; groupIndex < kIOFBNotifyGroupIndex_NumberOfGroups; groupIndex++)
            {
                deliverGroupNotification( gForwardGroup[groupIndex], eventMask, true, event, info );
                __private->fNotificationGroup |= (1 << groupIndex);
            }
            break;
        }
        // Reverse
        case kIOFBNotifyDisplayModeDidChange:
        case kIOFBNotifyDidWake:
        case kIOFBNotifyWillPowerOn:
        case kIOFBNotifyDidPowerOn:
        case kIOFBNotifyDidChangeSpeed:
        case kIOFBNotifyWSAAWillExitDefer:
        case kIOFBNotifyWSAADidExitDefer:
        case kIOFBNotifyDidNotify:
        case kIOFBNotifyHDACodecWillPowerOn:
        case kIOFBNotifyHDACodecDidPowerOn:
        {
            for (groupIndex = 0; groupIndex < kIOFBNotifyGroupIndex_NumberOfGroups; groupIndex++)
            {
                deliverGroupNotification( gReverseGroup[groupIndex], eventMask, false, event, info );
                __private->fNotificationGroup |= (1 << groupIndex);
            }
            break;
        }
        default:
        {
            D(NOTIFICATIONS, thisName, " Warning: unknown event type: %#x (%d : %c%c%c%c)\n", event, event, FEAT(event) );
            break;
        }
    }

#if RLOG1
#define STRING_NOTIFY(x)  case x: name = #x; break
    const auto deltams = at2ms(mach_absolute_time() - startTime);

    const char  * name = NULL;
    switch (event)
    {
        STRING_NOTIFY(kIOFBNotifyDisplayModeWillChange);
        STRING_NOTIFY(kIOFBNotifyDisplayModeDidChange);
        STRING_NOTIFY(kIOFBNotifyWillSleep);
        STRING_NOTIFY(kIOFBNotifyDidWake);
        STRING_NOTIFY(kIOFBNotifyDidPowerOff);
        STRING_NOTIFY(kIOFBNotifyWillPowerOn);
        STRING_NOTIFY(kIOFBNotifyWillPowerOff);
        STRING_NOTIFY(kIOFBNotifyDidPowerOn);
        STRING_NOTIFY(kIOFBNotifyWillChangeSpeed);
        STRING_NOTIFY(kIOFBNotifyDidChangeSpeed);
        STRING_NOTIFY(kIOFBNotifyDisplayDimsChange);
        STRING_NOTIFY(kIOFBNotifyClamshellChange);
        STRING_NOTIFY(kIOFBNotifyCaptureChange);
        STRING_NOTIFY(kIOFBNotifyOnlineChange);
        STRING_NOTIFY(kIOFBNotifyWSAAWillEnterDefer);
        STRING_NOTIFY(kIOFBNotifyWSAAWillExitDefer);
        STRING_NOTIFY(kIOFBNotifyWSAADidEnterDefer);
        STRING_NOTIFY(kIOFBNotifyWSAADidExitDefer);
        STRING_NOTIFY(kIOFBNotifyTerminated);
        STRING_NOTIFY(kIOFBNotifyHDACodecWillPowerOff);
        STRING_NOTIFY(kIOFBNotifyHDACodecDidPowerOff);
        STRING_NOTIFY(kIOFBNotifyHDACodecWillPowerOn);
        STRING_NOTIFY(kIOFBNotifyHDACodecDidPowerOn);
        default:
            name = "UNKNOWN kIONotifier";
            break;
    }
#undef STRING_NOTIFY

    bool notGated = (!gIOFBSystemWorkLoop->inGate() || !FBWL(this)->inGate());
    D(NOTIFICATIONS, thisName, " %s(%s(%d), %p) %qd ms\n",
          notGated ? "not gated " : "",
          name ? name : "", (uint32_t) event, OBFUSCATE(info),
          deltams);
#endif /* RLOG1 */

    __private->fNotificationActive = 0;

    switch (event)
    {
        // Too chatty or tracked via other traces
        case kIOFBNotifyDisplayDimsChange:
        case kIOFBNotifyWSAAWillEnterDefer:
        case kIOFBNotifyWSAAWillExitDefer:
        case kIOFBNotifyWSAADidEnterDefer:
        case kIOFBNotifyWSAADidExitDefer:
        case kIOFBNotifyWillChangeSpeed:
        case kIOFBNotifyDidChangeSpeed:
        case kIOFBNotifyWillSleep:
        case kIOFBNotifyDidWake:
        case kIOFBNotifyDidPowerOff:
        case kIOFBNotifyWillPowerOn:
        case kIOFBNotifyWillPowerOff:
        case kIOFBNotifyDidPowerOn:
        case kIOFBNotifyHDACodecWillPowerOn:    // Less interesting than did
        case kIOFBNotifyHDACodecDidPowerOff:    // Less interesting than will
        {
            break;
        }
        default:
        {
            IOG_KTRACE_NT(DBG_IOG_DELIVER_NOTIFY, DBG_FUNC_NONE,
                          __private->regID, event, ret, 0);
            break;
        }
    }

    UNLOCKNOTIFY();

    IOFB_END(deliverFramebufferNotification,ret,0,0);
    return (ret);
}

SInt32 IOFramebuffer::osNotifyOrderFunction( const OSMetaClassBase *obj1, const OSMetaClassBase *obj2, void *context)
{
    _IOFramebufferNotifier  * notify1 = OSDynamicCast(_IOFramebufferNotifier, obj1);
    _IOFramebufferNotifier  * notify2 = OSDynamicCast(_IOFramebufferNotifier, obj2);
    // Default to append
    SInt32  s32 = 1;

    D(NOTIFICATIONS, "OSF", " n1: %p, n2: %p\n", notify1, notify2);
    if ((NULL != notify1) && (NULL != notify2))
    {
        D(NOTIFICATIONS, "OSF", " p1: %d, p2: %d\n", notify1->fGroupPriority, notify2->fGroupPriority);
        if ((notify1->fGroupPriority < 0) && (notify2->fGroupPriority < 0))
        {
            s32 = notify2->fGroupPriority - notify1->fGroupPriority;
        }
        else
        {
            s32 = notify1->fGroupPriority - notify2->fGroupPriority;
        }
    }

    return (s32);
}

int32_t IOFramebuffer::groupIDToIndex( IOSelect groupID )
{
    int32_t setIndex = -1;

    switch (groupID)
    {
        case kIOFBNotifyGroupID_Legacy:
        {
            setIndex = kIOFBNotifyGroupIndex_Legacy;
            break;
        }
        case kIOFBNotifyGroupID_IODisplay:
        {
            setIndex = kIOFBNotifyGroupIndex_IODisplay;
            break;
        }
        case kIOFBNotifyGroupID_AppleGraphicsDevicePolicy:
        case kIOFBNotifyGroupID_AppleGraphicsMGPUPowerControl:
        case kIOFBNotifyGroupID_AppleGraphicsMUXControl:
        case kIOFBNotifyGroupID_AppleGraphicsControl:
        case kIOFBNotifyGroupID_AppleGraphicsDisplayPolicy:
        {
            setIndex = kIOFBNotifyGroupIndex_AppleGraphicsControl;
            break;
        }
        case kIOFBNotifyGroupID_AppleGraphicsPowerManagement:
        {
            setIndex = kIOFBNotifyGroupIndex_AppleGraphicsPowerManagement;
            break;
        }
        case kIOFBNotifyGroupID_AppleHDAController:
        {
            setIndex = kIOFBNotifyGroupIndex_AppleHDAController;
            break;
        }
        case kIOFBNotifyGroupID_AppleIOAccelDisplayPipe:
        {
            setIndex = kIOFBNotifyGroupIndex_AppleIOAccelDisplayPipe;
            break;
        }
        case kIOFBNotifyGroupID_AppleMCCSControl:
        {
            setIndex = kIOFBNotifyGroupIndex_AppleMCCSControl;
            break;
        }
        default :
        {
            if ((groupID >= kIOFBNotifyGroupID_VendorIntel) && (groupID <= (kIOFBNotifyGroupID_VendorIntel + 0xFF)))
            {
                setIndex = kIOFBNotifyGroupIndex_VendorIntel;
            }
            else if ((groupID >= kIOFBNotifyGroupID_VendorNVIDIA) && (groupID <= (kIOFBNotifyGroupID_VendorNVIDIA + 0xFF)))
            {
                setIndex = kIOFBNotifyGroupIndex_VendorNVIDIA;
            }
            else if ((groupID >= kIOFBNotifyGroupID_VendorAMD) && (groupID <= (kIOFBNotifyGroupID_VendorAMD + 0xFF)))
            {
                setIndex = kIOFBNotifyGroupIndex_VendorAMD;
            }
            else if (groupID >= 0x8000)
            {
                setIndex = kIOFBNotifyGroupIndex_ThirdParty;
            }
            break;
        }
    }

    return (setIndex);
}

IOSelect IOFramebuffer::eventToMask( IOIndex event )
{
    IOSelect    eventMask = 0;

    switch (event)
    {
        case kIOFBNotifyDisplayModeWillChange:
        case kIOFBNotifyDisplayModeDidChange:
        {
            eventMask = kIOFBNotifyEvent_DisplayModeChange;
            break;
        }
        case kIOFBNotifyWillSleep:
        case kIOFBNotifyDidWake:
        {
            eventMask = kIOFBNotifyEvent_SleepWake;
            break;
        }
        case kIOFBNotifyDidPowerOff:
        case kIOFBNotifyWillPowerOff:
        case kIOFBNotifyWillPowerOn:
        case kIOFBNotifyDidPowerOn:
        {
            eventMask = kIOFBNotifyEvent_PowerOnOff;
            break;
        }
        case kIOFBNotifyWillChangeSpeed:
        case kIOFBNotifyDidChangeSpeed:
        {
            eventMask = kIOFBNotifyEvent_ChangeSpeed;
            break;
        }
        case kIOFBNotifyWSAAWillEnterDefer:
        case kIOFBNotifyWSAAWillExitDefer:
        case kIOFBNotifyWSAADidEnterDefer:
        case kIOFBNotifyWSAADidExitDefer:
        {
            eventMask = kIOFBNotifyEvent_WSAADefer;
            break;
        }
        case kIOFBNotifyDisplayDimsChange:
        {
            eventMask = kIOFBNotifyEvent_DisplayDimsChange;
            break;
        }
        case kIOFBNotifyClamshellChange:
        {
            eventMask = kIOFBNotifyEvent_ClamshellChange;
            break;
        }
        case kIOFBNotifyCaptureChange:
        {
            eventMask = kIOFBNotifyEvent_CaptureChange;
            break;
        }
        case kIOFBNotifyOnlineChange:
        {
            eventMask = kIOFBNotifyEvent_OnlineChange;
            break;
        }
        case kIOFBNotifyProbed:
        {
            eventMask = kIOFBNotifyEvent_Probed;
            break;
        }
        case kIOFBNotifyVRAMReady:
        {
            eventMask = kIOFBNotifyEvent_VRAMReady;
            break;
        }
        case kIOFBNotifyWillNotify:
        case kIOFBNotifyDidNotify:
        {
            eventMask = kIOFBNotifyEvent_Notify;
            break;
        }
        case kIOFBNotifyTerminated:
        {
            eventMask = kIOFBNotifyEvent_Terminated;
            break;
        }
        case kIOFBNotifyHDACodecWillPowerOff:
        case kIOFBNotifyHDACodecDidPowerOff:
        case kIOFBNotifyHDACodecWillPowerOn:
        case kIOFBNotifyHDACodecDidPowerOn:
        {
            eventMask = kIOFBNotifyEvent_HDACodecPowerOnOff;
            break;
        }
        default:
        {
            eventMask = 0;
            break;
        }
    }

    return (eventMask);
}

void IOFramebuffer::deliverGroupNotification( int32_t targetIndex, IOSelect eventMask, bool bForward, IOIndex event, void * info )
{
    IOFB_START(deliverGroupNotification,0,0,0);
    _IOFramebufferNotifier  * notify = NULL;
    OSOrderedSet            * targetSet = NULL;
    IOReturn                r = kIOReturnSuccess;
    unsigned int            count = 0;
    unsigned int            totalCount = 0;
    unsigned int            setIndex = 0;
    uintptr_t               nameBufInt[4];
    uintptr_t               swpName[3];

    if (!fFBNotifications) {
        IOFB_END(deliverGroupNotification,-1,__LINE__,0);
        return;
    }

    D(NOTIFICATIONS, thisName, " Group: %#x, Mask: %#x, Forward: %s\n", targetIndex, eventMask, bForward ? "true" : "false" );

    targetSet = (OSOrderedSet *)fFBNotifications->getObject(targetIndex);
    targetSet = OSDynamicCast(OSOrderedSet, targetSet->copyCollection());
    if (NULL != targetSet)
    {
        totalCount = count = targetSet->getCount();
        while (count > 0)
        {
            if (bForward)
            {
                setIndex = (totalCount - count);
                count--;
            }
            else
            {
                count--;
                setIndex = count;
            }

            D(NOTIFICATIONS, thisName, " bForward: %s, Index: %d, Count: %d\n", bForward ? "true" : "false", setIndex, count);

            // Handle those clients that want Will/DidNotify events
            notify = (_IOFramebufferNotifier *)targetSet->getObject(setIndex);
            if (NULL != notify)
            {
                if ((false != notify->fEnable) && (targetIndex == notify->fGroup) && (eventMask & notify->fEvents))
                {
                    D(NOTIFICATIONS, thisName, " notify: %p, %#x, %d, %#x\n", notify, notify->fGroup, notify->fGroupPriority, notify->fEvents);

                    notify->fStampStart = mach_continuous_time();

                    IOFramebufferNotificationNotify hook;
                    hook.event = event;
                    hook.info  = info;

                    bzero(nameBufInt, sizeof(nameBufInt));
                    if (static_cast<bool>(notify->fName[0])) {
                        GPACKSTRING(nameBufInt, notify->fName);
                        swpName[0] = OSSwapBigToHostInt64(nameBufInt[0]);
                        swpName[1] = OSSwapBigToHostInt64(nameBufInt[1]);
                        swpName[2] = OSSwapBigToHostInt64(nameBufInt[2]);
                    }

                    if (notify->fEvents & kIOFBNotifyEvent_Notify)
                    {
                        // Will notify events
                        (*notify->fHandler)(notify->fTarget, notify->fRef, this, kIOFBNotifyWillNotify, &hook);

                        // Now the interested event.
                        IOFB_START(deliverFramebufferNotificationCallout,
                                   swpName[0],swpName[1],swpName[2]);
                        IOG_KTRACE_DEFER_START(
                                DBG_IOG_NOTIFY_CALLOUT_TIMEOUT,
                                DBG_FUNC_START,
                                0, __private->regID,
                                kGTRACE_ARGUMENT_STRING, nameBufInt[0],
                                kGTRACE_ARGUMENT_STRING, nameBufInt[1],
                                kGTRACE_ARGUMENT_STRING, nameBufInt[2]);
                        r = (*notify->fHandler)(notify->fTarget, notify->fRef, this, event, info );
                        IOG_KTRACE_DEFER_END(DBG_IOG_NOTIFY_CALLOUT_TIMEOUT,
                                             DBG_FUNC_END,
                                             0, event, 0, r, 0, 0, 0, 0,
                                             kNOTIFY_TIMEOUT_NS);
                        IOFB_END(deliverFramebufferNotificationCallout,r,0,0);
                        notify->fLastEvent = event;

                        // Did notify events
                        if (notify->fEnable)
                        {
                            (*notify->fHandler)(notify->fTarget, notify->fRef, this, kIOFBNotifyDidNotify, &hook);
                        }
                    }
                    else
                    {
                        IOFB_START(deliverFramebufferNotificationCallout,
                                   swpName[0],swpName[1],swpName[2]);
                        IOG_KTRACE_DEFER_START(
                                DBG_IOG_NOTIFY_CALLOUT_TIMEOUT,
                                DBG_FUNC_START,
                                0, __private->regID,
                                kGTRACE_ARGUMENT_STRING, nameBufInt[0],
                                kGTRACE_ARGUMENT_STRING, nameBufInt[1],
                                kGTRACE_ARGUMENT_STRING, nameBufInt[2]);
                        r = (*notify->fHandler)(notify->fTarget, notify->fRef, this, event, info );
                        IOG_KTRACE_DEFER_END(DBG_IOG_NOTIFY_CALLOUT_TIMEOUT,
                                             DBG_FUNC_END,
                                             0, event, 0, r, 0, 0, 0, 0,
                                             kNOTIFY_TIMEOUT_NS);
                        IOFB_END(deliverFramebufferNotificationCallout,r,0,0);
                        notify->fLastEvent = event;
                    }

                    notify->fStampEnd = mach_continuous_time();

                    if (kIOFBNotifyTerminated == event)
                    {
                        notify->disable();
                    }
                    const auto startTime\
                        = AbsoluteTime_to_scalar(&(notify->fStampStart));
                    const auto endTime
                        = AbsoluteTime_to_scalar(&(notify->fStampEnd));
                    const auto deltams = at2ms(endTime - startTime);
                    (void) deltams;
                    D(NOTIFICATIONS, thisName, " %#x(%#x) %p: %lld ms\n",
                      targetIndex, eventMask, OBFUSCATE(info), deltams);
                }
                else
                {
                    D(NOTIFICATIONS, thisName,
                      " INFO: notifier skipped: (%#x %#x), (%#x %#x) (%s)\n",
                      targetIndex, notify->fGroup, eventMask, notify->fEvents,
                      notify->fEnable ? "true" : "false");
                }
            }
            else
            {
                D(NOTIFICATIONS, thisName,
                  " INFO: notifier not found for target and index: (%#x %d)\n",
                  targetIndex, setIndex);
            }
        }

        OSSafeReleaseNULL(targetSet);
    }
    else
    {
        D(NOTIFICATIONS, thisName,
          " INFO: no set for target group: %#x\n", targetIndex );
    }

    IOFB_END(deliverGroupNotification,0,0,0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Some stubs

IOReturn IOFramebuffer::enableController ( void )
{
    IOFB_START(enableController,0,0,0);
    IOFB_END(enableController,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

bool IOFramebuffer::isConsoleDevice( void )
{
    IOFB_START(isConsoleDevice,0,0,0);
    IOFB_END(isConsoleDevice,false,0,0);
    return (false);
}

// Set display mode and depth
IOReturn IOFramebuffer::setDisplayMode( IODisplayModeID /* displayMode */,
                                        IOIndex /* depth */ )
{
    IOFB_START(setDisplayMode,0,0,0);
    IOFB_END(setDisplayMode,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

// For pages
IOReturn IOFramebuffer::setApertureEnable(
    IOPixelAperture /* aperture */, IOOptionBits /* enable */ )
{
    IOFB_START(setApertureEnable,0,0,0);
    IOFB_END(setApertureEnable,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

// Display mode and depth for startup
IOReturn IOFramebuffer::setStartupDisplayMode(
    IODisplayModeID /* displayMode */, IOIndex /* depth */ )
{
    IOFB_START(setStartupDisplayMode,0,0,0);
    IOFB_END(setStartupDisplayMode,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

IOReturn IOFramebuffer::getStartupDisplayMode(
    IODisplayModeID * /* displayMode */, IOIndex * /* depth */ )
{
    IOFB_START(getStartupDisplayMode,0,0,0);
    IOFB_END(getStartupDisplayMode,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

//// CLUTs

IOReturn IOFramebuffer::setCLUTWithEntries(
    IOColorEntry * /* colors */, UInt32 /* index */,
    UInt32 /* numEntries */, IOOptionBits /* options */ )
{
    IOFB_START(setCLUTWithEntries,0,0,0);
    IOFB_END(setCLUTWithEntries,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

//// Gamma
IOReturn IOFramebuffer::setGammaTable( UInt32 /* channelCount */,
                                       UInt32 /* dataCount */, UInt32 /* dataWidth */, void * /* data */,
                                       bool /* syncToVBL */)
{
    IOFB_START(setGammaTable2,0,0,0);
    IOFB_END(setGammaTable2,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

IOReturn IOFramebuffer::setGammaTable( UInt32 /* channelCount */,
                                       UInt32 /* dataCount */, UInt32 /* dataWidth */, void * /* data */ )
{
    IOFB_START(setGammaTable,0,0,0);
    IOFB_END(setGammaTable,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}


//// Display mode timing information

IOReturn IOFramebuffer::getTimingInfoForDisplayMode(
    IODisplayModeID /* displayMode */,
    IOTimingInformation * /* info */ )
{
    IOFB_START(getTimingInfoForDisplayMode,0,0,0);
    IOFB_END(getTimingInfoForDisplayMode,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

IOReturn IOFramebuffer::validateDetailedTiming(
    void * description, IOByteCount descripSize )
{
    IOFB_START(validateDetailedTiming,descripSize,0,0);
    IOFB_END(validateDetailedTiming,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

IOReturn IOFramebuffer::setDetailedTimings( OSArray * array )
{
    IOFB_START(setDetailedTimings,0,0,0);
    IOFB_END(setDetailedTimings,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

//// Connections

IOItemCount IOFramebuffer::getConnectionCount( void )
{
    IOFB_START(getConnectionCount,0,0,0);
    IOFB_END(getConnectionCount,1,0,0);
    return (1);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::setAttributeExt( IOSelect attribute, uintptr_t value )
{
    IOFB_START(setAttributeExt,attribute,value,0);
    IOReturn err = kIOReturnSuccess;

    GMETRICFUNC(DBG_IOG_SET_ATTRIBUTE_EXT, kGMETRICS_EVENT_START,
                kGMETRICS_DOMAIN_FRAMEBUFFER);
    IOG_KTRACE_NT(DBG_IOG_SET_ATTRIBUTE_EXT, DBG_FUNC_START,
                  __private->regID, attribute, value, 0);

    if (!SYSISLOCKED()) FBASSERTNOTGATED(this);
    SYSGATEGUARD(sysgated);
    FBGATEGUARD(ctrlgated, this);

    switch (attribute) {
        case kIOPowerAttribute:
            DEBG1(thisName, " mux power change %d->%ld, gated %d, thread %d\n",
                pendingPowerState, value,
                gIOFBSystemWorkLoop->inGate(), gIOFBSystemWorkLoop->onThread());


            if (value != pendingPowerState)
            {
                pendingPowerState = static_cast<unsigned int>(value);
                if (!__private->controllerIndex)
                {
                    __private->controller->fPendingMuxPowerChange = true;
                    __private->controller->startThread();
                }
            }
            err = kIOReturnSuccess;
            break;

        default:
            FB_START(setAttribute,attribute,__LINE__,value);
            err = setAttribute(attribute, value);
            FB_END(setAttribute,err,__LINE__,0);
            break;
    }

    IOG_KTRACE(DBG_IOG_SET_ATTRIBUTE_EXT, DBG_FUNC_END,
               0, __private->regID, 0, err, 0, 0, 0, 0);
    GMETRICFUNC(DBG_IOG_SET_ATTRIBUTE_EXT, kGMETRICS_EVENT_END,
                kGMETRICS_DOMAIN_FRAMEBUFFER);

    IOFB_END(setAttributeExt,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::getAttributeExt( IOSelect attribute, uintptr_t * value )
{
    IOFB_START(getAttributeExt,attribute,0,0);
	IOReturn err;

    FBGATEGUARD(ctrlgated, this);

    FB_START(getAttribute,attribute,__LINE__,0);
	err = getAttribute(attribute, value);
    FB_END(getAttribute,err,__LINE__,0);
	switch (attribute)
	{
		case kIOMirrorAttribute:
			if (kIOReturnSuccess != err) *value = 0;
			if (__private->nextMirror) *value |= kIOMirrorIsMirrored;
			err = kIOReturnSuccess;
			break;

		default:
			break;
	}
    IOFB_END(getAttributeExt,err,0,0);
	return (err);
}

IOReturn IOFramebuffer::setAttributeForConnectionExt( IOIndex connectIndex,
        IOSelect attribute, uintptr_t value )
{
    IOFB_START(setAttributeForConnectionExt,connectIndex,attribute,value);
    IOReturn err;
    uint16_t endTag = DBG_FUNC_NONE;

    if (!opened)
    {
        IOFB_END(setAttributeForConnectionExt,kIOReturnNotReady,0,0);
        return (kIOReturnNotReady);
    }

    FBGATEGUARD(ctrlgated, this);

    switch (attribute) {
        case kConnectionIgnore: {
            IOG_KTRACE_NT(DBG_IOG_AGC_MUTE, DBG_FUNC_NONE,
                GPACKUINT64T(__private->regID),
                GPACKUINT32T(0, value),
                GPACKBIT(0, __private->controller->isMuted()),
                0);
            DEBG1(thisName, " 0igr ->0x%lx, gated %d, thread %d\n", value,
                    gIOFBSystemWorkLoop->inGate(), gIOFBSystemWorkLoop->onThread());
            if (value & (1 << 31)) __private->controller->setState(kIOFBMuted);
            else                   __private->controller->clearState(kIOFBMuted);
            break;
        }
        case kConnectionProbe: {
            IOG_KTRACE_NT(DBG_IOG_SET_ATTR_FOR_CONN_EXT, DBG_FUNC_START,
                __private->regID, attribute, value, 0);
            endTag = DBG_FUNC_END;
            break;
        }
    }

    FB_START(setAttributeForConnection,attribute,__LINE__,value);
    err = setAttributeForConnection(connectIndex, attribute, value);
    FB_END(setAttributeForConnection,err,__LINE__,0);

    switch (attribute) {
        case kConnectionProbe: {
            const auto switching = gIOFBIsMuxSwitching;
            const auto muxed = static_cast<bool>(__private->controller->fAliasID);
            const auto fb0 = (0 == __private->controllerIndex);
            const auto muted = __private->controller->isMuted();
            const auto offline = (0 == __private->controller->fOnlineMask);
            if (switching && muxed && fb0 && (muted != offline)) {
                if (offline) __private->controller->fMuxNeedsBgOn = true;
                __private->controller->fConnectChangeForMux = true;
                connectChangeInterruptImpl(DBG_IOG_SOURCE_SET_ATTR_FOR_CONN_EXT);
            }
            break;
        }
    }

    IOG_KTRACE_NT(DBG_IOG_SET_ATTR_FOR_CONN_EXT, endTag,
        __private->regID, attribute, value, err);

    IOFB_END(setAttributeForConnectionExt,err,0,0);
    return err;
}

IOReturn IOFramebuffer::getAttributeForConnectionExt( IOIndex connectIndex,
        IOSelect attribute, uintptr_t * value )
{
    IOFB_START(getAttributeForConnectionExt,connectIndex,attribute,0);
	IOReturn err;

    FBGATEGUARD(ctrlgated, this);

    FB_START(getAttributeForConnection,attribute,__LINE__,0);
	err = getAttributeForConnection(connectIndex, attribute, value);
    FB_END(getAttributeForConnection,err,__LINE__,0);

    IOFB_END(getAttributeForConnectionExt,err,0,0);
	return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::getAttributeForConnectionParam( IOIndex connectIndex,
        IOSelect attribute, uintptr_t * value )
{
    IOFB_START(getAttributeForConnectionParam,connectIndex,attribute,0);
	IOReturn err;

	if (!__private->colorModesAllowed)
	{
		if (kConnectionColorMode == attribute)
        {
            IOFB_END(getAttributeForConnectionParam,kIOReturnUnsupported,__LINE__,0);
			return (kIOReturnUnsupported);
        }
		if (kConnectionColorModesSupported == attribute)
        {
            IOFB_END(getAttributeForConnectionParam,kIOReturnUnsupported,__LINE__,0);
			return (kIOReturnUnsupported);
        }
	}

    FB_START(getAttributeForConnection,attribute,__LINE__,0);
	err = getAttributeForConnection(connectIndex, attribute, value);
    FB_END(getAttributeForConnection,err,__LINE__,0);

    IOFB_END(getAttributeForConnectionParam,err,0,0);
	return (err);
}

IOReturn IOFramebuffer::setAttributeForConnectionParam( IOIndex connectIndex,
           IOSelect attribute, uintptr_t value )
{
    IOFB_START(setAttributeForConnectionParam,connectIndex,attribute,value);
	IOReturn err;

    FB_START(setAttributeForConnection,attribute,__LINE__,value);
	err = setAttributeForConnection(connectIndex, attribute, value);
    FB_END(setAttributeForConnection,err,__LINE__,0);

    IOFB_END(setAttributeForConnectionParam,err,0,0);
	return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::setAttributeForConnection( IOIndex connectIndex,
        IOSelect attribute, uintptr_t info )
{
    IOFB_START(setAttributeForConnection,connectIndex,attribute,0);
    IOReturn err;

    switch( attribute )
    {
        case kConnectionVBLMultiplier:
            if (info != gIOFBVblDeltaMult)
            {
                gIOFBVblDeltaMult = info;
                triggerEvent(kIOFBEventVBLMultiplier);
            }
            err = kIOReturnSuccess;
            break;

        case kConnectionRedGammaScale:
            if (info != __private->gammaScale[0])
            {
                __private->gammaScale[0] = info;
                __private->gammaScaleChange = true;
            }
            err = kIOReturnSuccess;
            break;

        case kConnectionGreenGammaScale:
            if (info != __private->gammaScale[1])
            {
                __private->gammaScale[1] = info;
                __private->gammaScaleChange = true;
            }
            err = kIOReturnSuccess;
            break;

        case kConnectionBlueGammaScale:
            if (info != __private->gammaScale[2])
            {
                __private->gammaScale[2] = info;
                __private->gammaScaleChange = true;
            }
            err = kIOReturnSuccess;
            break;

        case kConnectionGammaScale:
            if (info != __private->gammaScale[3])
            {
                __private->gammaScale[3] = info;
                __private->gammaScaleChange = true;
            }
            err = kIOReturnSuccess;
            break;

        case kConnectionFlushParameters:
			if (__private->gammaScaleChange)
			{
				__private->gammaScaleChange = false;
                updateGammaTable(__private->rawGammaChannelCount,
                                 __private->rawGammaDataCount,
                                 __private->rawGammaDataWidth,
                                 __private->rawGammaData,
                                 kIOFBSetGammaSyncNotSpecified,
                                 false, /*immediate*/
                                 false /*ignoreTransactionActive*/);
			}
            err = kIOReturnSuccess;
            break;

        case kConnectionAudioStreaming:
			__private->audioStreaming = info;
			pendingPowerChange = true;
			__private->controller->startThread();
            err = kIOReturnSuccess;
            break;

        case kConnectionOverscan:

            UInt64 newTransform;
            DEBG(thisName, " set oscn %ld, ena %d\n", info, __private->enableScalerUnderscan);
            if (info) 
                newTransform = __private->selectedTransform & ~kIOFBScalerUnderscan;
            else
                newTransform = __private->selectedTransform | kIOFBScalerUnderscan;
            if (__private->enableScalerUnderscan)
            {
                if (newTransform != __private->selectedTransform)
                {
                    __private->selectedTransform = newTransform;
                    if (!suspended)
                        connectChangeInterruptImpl(DBG_IOG_SOURCE_OVERSCAN);
                    else
                    {
                        __private->transform = newTransform;
                        setProperty(kIOFBTransformKey, newTransform, 64);
                    }
                }
                err = kIOReturnSuccess;
                break;
            }

            /* fall thru */

        default:
            err = kIOReturnUnsupported;
            break;
    }

    IOFB_END(setAttributeForConnection,err,0,0);
    return( err );
}

IOReturn IOFramebuffer::getAttributeForConnection( IOIndex connectIndex,
        IOSelect attribute, uintptr_t * value )
{
    IOFB_START(getAttributeForConnection,connectIndex,attribute,0);
    IOReturn err;
    uintptr_t result;

    switch( attribute )
    {
        case kConnectionDisplayParameterCount:
            result = 3;		         // 3 gamma rgb scales
			if (gIOGFades) result++; // 1 gamma scale
			result++;				 // vbl mult
            if (__private->enableScalerUnderscan)
                result++;
            *value = result;
            err = kIOReturnSuccess;
            break;

        case kConnectionDisplayParameters:
			result = 0;
            value[result++] = kConnectionRedGammaScale;
            value[result++] = kConnectionGreenGammaScale;
            value[result++] = kConnectionBlueGammaScale;
            if (gIOGFades) value[result++] = kConnectionGammaScale;
			value[result++] = kConnectionVBLMultiplier;
            if (__private->enableScalerUnderscan)
                value[result++] = kConnectionOverscan;
            err = kIOReturnSuccess;
            break;

        case kConnectionOverscan:
            if (__private->enableScalerUnderscan)
            {
                value[0] = (0 == (kIOFBScalerUnderscan & __private->selectedTransform));
                DEBG(thisName, " oscn %ld (%qx)\n", value[0], __private->selectedTransform);
                value[1] = 0;
                value[2] = 1;
                err = kIOReturnSuccess;
            }
            else
                err = kIOReturnUnsupported;
            break;

        case kConnectionVBLMultiplier:
            value[0] = gIOFBVblDeltaMult;
            value[1] = 0;
            value[2] = (3 << 16);
            err = kIOReturnSuccess;
            break;

        case kConnectionRedGammaScale:
            value[0] = __private->gammaScale[0];
            value[1] = 0;
            value[2] = (1 << 16);
            err = kIOReturnSuccess;
            break;

        case kConnectionGreenGammaScale:
            value[0] = __private->gammaScale[1];
            value[1] = 0;
            value[2] = (1 << 16);
            err = kIOReturnSuccess;
            break;

        case kConnectionBlueGammaScale:
            value[0] = __private->gammaScale[2];
            value[1] = 0;
            value[2] = (1 << 16);
            err = kIOReturnSuccess;
            break;

        case kConnectionGammaScale:
            value[0] = __private->gammaScale[3];
            value[1] = 0;
            value[2] = (1 << 16);
            err = kIOReturnSuccess;
            break;

        case kConnectionCheckEnable:
            FB_START(getAttributeForConnection,kConnectionEnable,__LINE__,0);
            err = getAttributeForConnection(connectIndex, kConnectionEnable, value);
            IOG_KTRACE_NT(DBG_IOG_CONNECTION_ENABLE, DBG_FUNC_NONE,
                       __private->regID, *value, err, 0);
            FB_END(getAttributeForConnection,err,__LINE__,0);
            break;

        case kConnectionSupportsHLDDCSense:
            if (__private->lli2c)
            {
                err = kIOReturnSuccess;
                break;
            }
            // fall thru
        default:
            err = kIOReturnUnsupported;
            break;
    }

    IOFB_END(getAttributeForConnection,err,0,0);
    return( err );
}

//// HW Cursors

IOReturn IOFramebuffer::setCursorImage( void * cursorImage )
{
    IOFB_START(setCursorImage,0,0,0);
    IOFB_END(setCursorImage,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

IOReturn IOFramebuffer::setCursorState( SInt32 x, SInt32 y, bool visible )
{
    IOFB_START(setCursorState,x,y,visible);
    IOFB_END(setCursorState,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

void IOFramebuffer::flushCursor( void )
{
    IOFB_START(flushCursor,0,0,0);
    IOFB_END(flushCursor,0,0,0);
}

//// Interrupts

IOReturn IOFramebuffer::registerForInterruptType( IOSelect interruptType,
        IOFBInterruptProc proc, OSObject * target, void * ref,
        void ** interruptRef )
{
    IOFB_START(registerForInterruptType,interruptType,0,0);
    if ((interruptType != kIOFBMCCSInterruptType) || !__private->dpInterrupts)
    {
        IOFB_END(registerForInterruptType,kIOReturnNoResources,0,0);
		return (kIOReturnNoResources);
    }

	interruptType = kIOFBMCCSInterruptRegister;
	__private->interruptRegisters[interruptType].handler = proc;
	__private->interruptRegisters[interruptType].target  = target;
	__private->interruptRegisters[interruptType].ref     = ref;
	__private->interruptRegisters[interruptType].state   = true;
	*interruptRef = &__private->interruptRegisters[interruptType];

    IOFB_END(registerForInterruptType,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::unregisterInterrupt(void * interruptRef)
{
    IOFB_START(unregisterInterrupt,0,0,0);
    IOReturn  err = kIOReturnBadArgument;

    const auto intRef = static_cast<IOFBInterruptRegister *>(interruptRef);
    if (NULL != intRef)
    {
        if (intRef == &__private->interruptRegisters[kIOFBMCCSInterruptRegister])
        {
            __private->interruptRegisters[kIOFBMCCSInterruptRegister].handler = 0;
            err = kIOReturnSuccess;
        }
        else
        {
            err = kIOReturnUnsupported;
        }
    }

    IOFB_END(unregisterInterrupt,err,0,0);
    return (err);
}

IOReturn IOFramebuffer::setInterruptState(void * interruptRef, UInt32 state)
{
    IOFB_START(setInterruptState,state,0,0);
    IOReturn  err = kIOReturnBadArgument;

    const auto intRef = static_cast<IOFBInterruptRegister *>(interruptRef);
    if (NULL != intRef)
    {
        if (intRef == &__private->interruptRegisters[kIOFBMCCSInterruptRegister])
        {
            __private->interruptRegisters[kIOFBMCCSInterruptRegister].state = state;
            err = kIOReturnSuccess;
        }
        else
        {
            err = kIOReturnUnsupported;
        }
    }

    IOFB_END(setInterruptState,err,0,0);
    return (err);
}

// Apple sensing

IOReturn IOFramebuffer::getAppleSense(
    IOIndex  /* connectIndex */,
    UInt32 * /* senseType */,
    UInt32 * /* primary */,
    UInt32 * /* extended */,
    UInt32 * /* displayType */ )
{
    IOFB_START(getAppleSense,0,0,0);
    IOFB_END(getAppleSense,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

IOReturn IOFramebuffer::connectFlags( IOIndex /* connectIndex */,
                                      IODisplayModeID /* displayMode */, IOOptionBits * /* flags */ )
{
    IOFB_START(connectFlags,0,0,0);
    IOFB_END(connectFlags,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

//// IOLowLevelDDCSense

void IOFramebuffer::setDDCClock( IOIndex /* connectIndex */, UInt32 /* value */ )
{
    IOFB_START(setDDCClock,0,0,0);
    IOFB_END(setDDCClock,0,0,0);
}

void IOFramebuffer::setDDCData( IOIndex /* connectIndex */, UInt32 /* value */ )
{
    IOFB_START(setDDCData,0,0,0);
    IOFB_END(setDDCData,0,0,0);
}

bool IOFramebuffer::readDDCClock( IOIndex /* connectIndex */ )
{
    IOFB_START(readDDCClock,0,0,0);
    IOFB_END(readDDCClock,false,0,0);
    return (false);
}

bool IOFramebuffer::readDDCData( IOIndex /* connectIndex */ )
{
    IOFB_START(readDDCData,0,0,0);
    IOFB_END(readDDCData,false,0,0);
    return (false);
}

IOReturn IOFramebuffer::enableDDCRaster( bool /* enable */ )
{
    IOFB_START(enableDDCRaster,0,0,0);
    IOFB_END(enableDDCRaster,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//// IOHighLevelDDCSense

enum { kDDCBlockSize = 128 };

bool IOFramebuffer::hasDDCConnect( IOIndex connectIndex )
{
    IOFB_START(hasDDCConnect,connectIndex,0,0);
    bool b = (__private->lli2c);
    IOFB_END(hasDDCConnect,b,0,0);
    return (b);
}

IOReturn IOFramebuffer::getDDCBlock( IOIndex bus, UInt32 blockNumber,
                                        IOSelect blockType, IOOptionBits options,
                                        UInt8 * data, IOByteCount * length )
{
    IOFB_START(getDDCBlock,bus,blockNumber,blockType);
    UInt8               startAddress;
    IOReturn            err;
    UInt32              badsums, timeouts;
    IOI2CBusTiming *    timing = &__private->defaultI2CTiming;

    if (!__private->lli2c)
    {
        IOFB_END(getDDCBlock,kIOReturnUnsupported,0,0);
        return (kIOReturnUnsupported);
    }
    
    // Assume that we have already attempted to stop DDC1
    
    // Read the requested block (Block 1 is at 0x0, each additional block is at 0x80 offset)
    startAddress = kDDCBlockSize * (blockNumber - 1);
    if (length)
        *length = kDDCBlockSize;
    
    // Attempt to read the DDC data
    //  1.      If the error is a timeout, then it will attempt one more time.  If it gets another timeout, then
    //          it will return a timeout error to the caller.
    //
    //  2.  If the error is a bad checksum error, it will attempt to read the block up to 2 more times.
    //          If it still gets an error, then it will return a bad checksum error to the caller.  
    i2cSend9Stops(bus, timing);
    badsums = timeouts = 0;
    do
    {
        err = readDDCBlock(bus, timing, 0xa0, startAddress, data);
        if (kIOReturnSuccess == err)
            break;
        IOLog("readDDCBlock returned error\n");
        i2cSend9Stops(bus, timing);

        // We got an error.   Determine what kind
        if (kIOReturnNotResponding == err)
        {
            IOLog("timeout\n");
            timeouts++;
        }
        else if (kIOReturnUnformattedMedia == err)
        {
            IOLog("bad sum\n");
            badsums++;
        }
        else
            break;
    }
    while ((timeouts < 2) && (badsums < 4));

    IOFB_END(getDDCBlock,err,0,0);
    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      doI2CRequest(), 
//

IOReturn IOFramebuffer::doI2CRequest( UInt32 bus, IOI2CBusTiming * timing, IOI2CRequest * request )
{
    IOFB_START(doI2CRequest,bus,0,0);
    IOReturn err = kIOReturnError;      // Assume failure

    if (!__private->lli2c)
    {
        IOFB_END(doI2CRequest,kIOReturnUnsupported,0,0);
        return (kIOReturnUnsupported);
    }

    if (!timing)
        timing = &__private->defaultI2CTiming;
    
    if (request->sendTransactionType == kIOI2CSimpleTransactionType)
    {
        if ( request->sendAddress & 0x01 )
        {
            // Read Transaction
            //
            err = i2cReadData(bus, timing, request->sendAddress, request->sendBytes, (UInt8 *) request->sendBuffer);
        }
        else
        {
            // Read Transaction
            //
            err = i2cWriteData(bus, timing, request->sendAddress, request->sendBytes, (UInt8 *) request->sendBuffer);
        }
    }

    // Now, let's check to see if there is a csReplyType
    //
    if (request->replyTransactionType == kIOI2CDDCciReplyTransactionType )
    {
        err = i2cReadDDCciData(bus, timing, request->replyAddress, request->replyBytes, (UInt8 *) request->replyBuffer);
    }
    else if (request->replyTransactionType == kIOI2CSimpleTransactionType )
    {
        err = i2cReadData(bus, timing, request->replyAddress, request->replyBytes, (UInt8 *) request->replyBuffer);
    }

    request->result = err;
    if (request->completion)
        (*request->completion)(request);

    err = kIOReturnSuccess;

    IOFB_END(doI2CRequest,err,0,0);
    return (err);
}

/*
    with thanks to:
    File:       GraphicsCoreUtils.c
    Written by: Sean Williams, Kevin Williams, Fernando Urbina
*/

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      stopDDC1SendCommand(), 
//
//      The VESA spec for DDC says a display in DDC1 will transition from DDC1 to DDC2 when a valid DDC2
//      command is received.
//      DDC1 constantly spews data on the data line if syncs are active...bad for macintosh sensing
//      DDC2 only sends data when requested.
//      The VESA spec illustrates the manner to do this.
//      Read the first byte of data, send a Nack and a Stop.
//      This routine does that.
//
//      There is a delay of two vertical clock periods where the clock line is forced low. The 
//      NEC XE15 monitor has a controller that sometimes pulls the clockline low and never releases it.
//      This is bad, DDC will fail, and the monitor sensing algorithim will think a mono 1152x870 display
//      is attached. This isn't part of the spec but it fixes the NEC XE15.
//

IOReturn IOFramebuffer::stopDDC1SendCommand(IOIndex bus, IOI2CBusTiming * timing)
{
    IOFB_START(stopDDC1SendCommand,bus,0,0);
    UInt8       data;
    IOReturn    err = kIOReturnSuccess; 
    UInt8       address;

    // keep clock line low for 2 vclocks....keeps NEC XE15 from locking clock line low
    // 640x480@67hz has a veritcal frequency of 15 ms
    // 640x480@60hz has a vertical frequency of 16.7 ms
    // Lower the clock line for 34 milliseconds

    FB_START(setDDCClock,kIODDCLow,__LINE__,0);
    setDDCClock(bus, kIODDCLow);
    FB_END(setDDCClock,0,__LINE__,0);
    IOSleep( 34 );

    address = 0;
    err = i2cWrite(bus, timing, 0xa0, 1, &address);

    if (kIOReturnSuccess == err)
    {                   
        i2cStart(bus, timing);
        
        i2cSendByte(bus, timing, 0xa1 );
        
        err = i2cWaitForAck(bus, timing);
        if (kIOReturnSuccess == err)
            err = i2cReadByte(bus, timing, &data);
    }
    
    i2cSendNack(bus, timing);
    i2cStop(bus, timing);

    IOFB_END(stopDDC1SendCommand,err,0,0);
    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cReadData()
//
//      The parameters are described as follows:
//
//                      -> deviceAddress        device's I2C address
//                      -> count                # of bytes to read
//                      <- buffer               buffer for the data

IOReturn IOFramebuffer::i2cReadData(IOIndex bus, IOI2CBusTiming * timing,
                                    UInt8 deviceAddress, UInt8 count, UInt8 * buffer)
{
    IOFB_START(i2cReadData,bus,deviceAddress,count);
    IOReturn    err = kIOReturnError;
    UInt32      attempts = 10;
    
    while ((kIOReturnSuccess != err) && (attempts-- > 0))
    {
        // Attempt to read the I2C data
        i2cSend9Stops(bus, timing);
        err = i2cRead(bus, timing, deviceAddress, count, buffer);
    }
    
    IOFB_END(i2cReadData,err,0,0);
    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cWriteData()
//
//      The parameters are described as follows:
//
//                      -> deviceAddress        device's I2C address
//                      -> count                # of bytes to read
//                      -> buffer               buffer for the data

IOReturn IOFramebuffer::i2cWriteData(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 count, UInt8 * buffer)
{
    IOFB_START(i2cWriteData,bus,deviceAddress,count);
    IOReturn    err = kIOReturnError;
    UInt32      attempts = 10;
    
    while ((kIOReturnSuccess != err) && (attempts-- > 0))
    {
        // Attempt to write the I2C data
        i2cSend9Stops(bus, timing);
        err = i2cWrite(bus, timing, deviceAddress, count, buffer);
    }
    
    IOFB_END(i2cWriteData,err,0,0);
    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      WaitForDDCDataLine()
//
//      Watch the DDC data line and see if it is toggling. If the data line is toggling, it means
//      1) DDC display is connected
//      2) DDC controller in the display is ready to receive commands.
//
//                      -> waitTime             max duration that the DDC data line should be watched

void IOFramebuffer::waitForDDCDataLine(IOIndex bus, IOI2CBusTiming * timing, UInt32 waitTime)
{
    IOFB_START(waitForDDCDataLine,bus,waitTime,0);
    AbsoluteTime        now, expirationTime;
    UInt32              dataLine;

    FB_START(setDDCData,kIODDCTristate,__LINE__,0);
    setDDCData(bus, kIODDCTristate);            // make sure data line is tristated
    FB_END(setDDCData,0,__LINE__,0);

    // Set up the timeout timer...watch DDC data line for waitTime, see if it changes
    clock_interval_to_deadline(waitTime, kMillisecondScale, &expirationTime);
                            
    FB_START(readDDCData,bus,__LINE__,0);
    dataLine = readDDCData(bus);                // read present state of dataline
    FB_END(readDDCData,0,__LINE__,0);

    while (true)
    {
        FB_START(readDDCData,bus,__LINE__,0);
        if (dataLine != readDDCData(bus))
        {
            FB_END(readDDCData,0,__LINE__,0);
            break;
        }
        FB_END(readDDCData,0,__LINE__,0);

        AbsoluteTime_to_scalar(&now) = mach_absolute_time();
        if (CMP_ABSOLUTETIME(&now, &expirationTime) > 0)
            break;
    }
    IOFB_END(waitForDDCDataLine,0,0,0);
}
        
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      readDDCBlock()
//      Read one block of DDC data
//
//      The parameters are described as follows:
//
//                      -> deviceAddress        device's I2C address
//                      -> startAddress         start address to get data from
//                      <- data                 a block of EDID data

IOReturn IOFramebuffer::readDDCBlock(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 startAddress, UInt8 * data)
{
    IOFB_START(readDDCBlock,bus,deviceAddress,startAddress);
    IOReturn    err;
    UInt32      i;
    UInt8       sum = 0;
    
    // First, send the address/data as a write
    err = i2cWrite(bus, timing, deviceAddress, 1, &startAddress);
    if (kIOReturnSuccess != err)
        goto ErrorExit;
    
    // Now, read the I2C data
    err = i2cRead(bus, timing, deviceAddress, kDDCBlockSize, data);
    if (kIOReturnSuccess != err)
        goto ErrorExit;
    
    for (i = 0; i < kDDCBlockSize; i++)
        sum += data[i];

    if (sum)
        err = kIOReturnUnformattedMedia;
    
ErrorExit:

    IOFB_END(readDDCBlock,err,0,0);
    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cStart()
//      Start a I2C transaction

void IOFramebuffer::i2cStart(IOIndex bus, IOI2CBusTiming * timing)
{
    IOFB_START(i2cStart,bus,0,0);
    // Generates a Start condition:
    
    // Set DATA and CLK high and enabled
    FB_START(setDDCData,kIODDCHigh,__LINE__,0);
    setDDCData(bus, kIODDCHigh);
    FB_END(setDDCData,0,__LINE__,0);
    FB_START(setDDCClock,kIODDCHigh,__LINE__,0);
    setDDCClock(bus, kIODDCHigh);
    FB_END(setDDCClock,0,__LINE__,0);

    IODelay( 100 );
    
    // Bring DATA low
    FB_START(setDDCData,kIODDCLow,__LINE__,0);
    setDDCData(bus, kIODDCLow);
    FB_END(setDDCData,0,__LINE__,0);
    IODelay( 100 );
    
    // Bring CLK low
    FB_START(setDDCClock,kIODDCLow,__LINE__,0);
    setDDCClock(bus, kIODDCLow);
    FB_END(setDDCClock,0,__LINE__,0);
    IODelay( 100 );
    IOFB_END(i2cStart,0,0,0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cStop()

void IOFramebuffer::i2cStop(IOIndex bus, IOI2CBusTiming * timing)
{
    IOFB_START(i2cStop,bus,0,0);
    // Generate a low to high transition on DATA
    // while SCL is high
    
    // Bring DATA and CLK low
    IODelay( 200 );
    FB_START(setDDCData,kIODDCLow,__LINE__,0);
    setDDCData(bus, kIODDCLow);
    FB_END(setDDCData,0,__LINE__,0);
    FB_START(setDDCClock,kIODDCLow,__LINE__,0);
    setDDCClock(bus, kIODDCLow);
    FB_END(setDDCClock,0,__LINE__,0);

    IODelay( 100 );
    
    // Bring CLK High
    FB_START(setDDCClock,kIODDCHigh,__LINE__,0);
    setDDCClock(bus, kIODDCHigh);
    FB_END(setDDCClock,0,__LINE__,0);
    IODelay( 200 );
    
    // Bring DATA High
    FB_START(setDDCData,kIODDCHigh,__LINE__,0);
    setDDCData(bus, kIODDCHigh);
    FB_END(setDDCData,0,__LINE__,0);
    IODelay( 100 );

    // Release Bus

    FB_START(setDDCData,kIODDCTristate,__LINE__,0);
    setDDCData(bus, kIODDCTristate);
    FB_END(setDDCData,0,__LINE__,0);
    FB_START(setDDCClock,kIODDCTristate,__LINE__,0);
    setDDCClock(bus, kIODDCTristate);
    FB_END(setDDCClock,0,__LINE__,0);
    IOFB_END(i2cStop,0,0,0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cSendAck()
//      Send an ACK to acknowledge we received the data

void IOFramebuffer::i2cSendAck(IOIndex bus, IOI2CBusTiming * timing)
{
    IOFB_START(i2cSendAck,bus,0,0);
    // Here, we have to make sure that the CLK is low while
    // we bring DATA low and pulse CLK
    FB_START(setDDCClock,kIODDCLow,__LINE__,0);
    setDDCClock(bus, kIODDCLow);
    FB_END(setDDCClock,0,__LINE__,0);

    // This routine will release the bus by
    // tristating the CLK and DATA lines
    IODelay(20);

    // should we wait for the SDA to be high before going on???

    IODelay( 40 );

    // Bring SDA low
    FB_START(setDDCData,kIODDCLow,__LINE__,0);
    setDDCData(bus, kIODDCLow);
    FB_END(setDDCData,0,__LINE__,0);
    IODelay( 100 );
    
    // pulse the CLK
    FB_START(setDDCClock,kIODDCHigh,__LINE__,0);
    setDDCClock(bus, kIODDCHigh);
    FB_END(setDDCClock,0,__LINE__,0);
    IODelay( 200 );
    FB_START(setDDCClock,kIODDCLow,__LINE__,0);
    setDDCClock(bus, kIODDCLow);
    FB_END(setDDCClock,0,__LINE__,0);
    IODelay( 40 );
    
    // Release SDA,
    FB_START(setDDCData,kIODDCTristate,__LINE__,0);
    setDDCData(bus, kIODDCTristate);
    FB_END(setDDCData,0,__LINE__,0);
    IOFB_END(i2cSendAck,0,0,0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cSendNack()
//      Send an ACK to acknowledge we received the data

void IOFramebuffer::i2cSendNack(IOIndex bus, IOI2CBusTiming * timing)
{
    IOFB_START(i2cSendNack,bus,0,0);
    // Here, we have to make sure that the CLK is low while
    // we bring DATA high and pulse CLK
    FB_START(setDDCClock,kIODDCLow,__LINE__,0);
    setDDCClock(bus, kIODDCLow);
    FB_END(setDDCClock,0,__LINE__,0);

    // This routine will release the bus by
    // tristating the CLK and DATA lines
    IODelay( 20 );
    // should we wait for the SDA to be high before going on???

    IODelay( 40 );

    // Bring SDA high
    FB_START(setDDCData,kIODDCHigh,__LINE__,0);
    setDDCData(bus, kIODDCHigh);
    FB_END(setDDCData,0,__LINE__,0);
    IODelay( 100 );
    
    // pulse the CLK
    FB_START(setDDCClock,kIODDCHigh,__LINE__,0);
    setDDCClock(bus, kIODDCHigh);
    FB_END(setDDCClock,0,__LINE__,0);
    IODelay( 200 );
    FB_START(setDDCClock,kIODDCLow,__LINE__,0);
    setDDCClock(bus, kIODDCLow);
    FB_END(setDDCClock,0,__LINE__,0);
    IODelay( 40 );
    
    // Release SDA,
    FB_START(setDDCData,kIODDCTristate,__LINE__,0);
    setDDCData(bus, kIODDCTristate);
    FB_END(setDDCData,0,__LINE__,0);
    IODelay( 100 );
    IODelay( 100 );
    IOFB_END(i2cSendNack,0,0,0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cWaitForAck()
//      This routine will poll the SDA line looking for a LOW value and when it finds it, it will pulse
//      the CLK.

IOReturn IOFramebuffer::i2cWaitForAck(IOIndex bus, IOI2CBusTiming * timing)
{
    IOFB_START(i2cWaitForAck,bus,0,0);
    uint64_t expirationTime;
    IOReturn err = kIOReturnSuccess;
    
    IODelay( 40 );
    
    // Set up a watchdog timer that will time us out, in case we never see the SDA LOW.
    clock_interval_to_deadline(1, kMillisecondScale, &expirationTime);

    FB_START(readDDCData,bus,__LINE__,0);
    while ((0 != readDDCData(bus)) && (kIOReturnSuccess == err))
    {
        const auto now = mach_absolute_time();
        if (now > expirationTime)
            err = kIOReturnNotResponding;                               // Timed Out
    }
    FB_END(readDDCData,err,__LINE__,0);

    // OK, now pulse the clock (SDA is not enabled), the CLK
    // should be low here.
    IODelay( 40 );
    FB_START(setDDCClock,kIODDCHigh,__LINE__,0);
    setDDCClock(bus, kIODDCHigh);
    FB_END(setDDCClock,0,__LINE__,0);
    IODelay( 200 );
    FB_START(setDDCClock,kIODDCLow,__LINE__,0);
    setDDCClock(bus, kIODDCLow);
    FB_END(setDDCClock,0,__LINE__,0);
    IODelay( 40 );

    IOFB_END(i2cWaitForAck,err,0,0);
    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cSendByte()
//      Send a byte of data
//

void IOFramebuffer::i2cSendByte(IOIndex bus, IOI2CBusTiming * timing, UInt8 data)
{
    IOFB_START(i2cSendByte,bus,data,0);
    UInt8       valueToSend;
    int         i;
    
    // CLK should be low when entering this routine
    // and will be low when exiting
    
    for ( i = 0 ; i < 8; i++ )
    {
        // Wait a bit
        IODelay( 100 );

        // Get the bit
        valueToSend = ( data >> (7 - i)) & 0x01;

        // Send it out
        FB_START(setDDCData,valueToSend,__LINE__,0);
        setDDCData(bus, valueToSend);
        FB_END(setDDCData,0,__LINE__,0);

        // Wait for 40 us and then pulse the clock
        
        IODelay( 40 );
        // Raise the CLK line
        FB_START(setDDCClock,kIODDCHigh,__LINE__,0);
        setDDCClock(bus, kIODDCHigh);
        FB_END(setDDCClock,0,__LINE__,0);

        IODelay( 200 );
        // Lower the clock line
        FB_START(setDDCClock,kIODDCLow,__LINE__,0);
        setDDCClock(bus, kIODDCLow);
        FB_END(setDDCClock,0,__LINE__,0);

        // Wait a bit
        IODelay( 40 );
    }
    
    // Tristate the DATA while keeping CLK low
    FB_START(setDDCData,kIODDCTristate,__LINE__,0);
    setDDCData(bus, kIODDCTristate);
    FB_END(setDDCData,0,__LINE__,0);
    IOFB_END(i2cSendByte,0,0,0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cReadByte()
//      Read a byte of data

IOReturn IOFramebuffer::i2cReadByte(IOIndex bus, IOI2CBusTiming * timing, UInt8 *data)
{
    IOFB_START(i2cReadByte,bus,0,0);
    AbsoluteTime        now, expirationTime;
    IOReturn            err = kIOReturnSuccess;
    UInt32              i;
    UInt32              value;

    // Make sure that DATA is Tristated and that Clock is low
    FB_START(setDDCClock,kIODDCLow,__LINE__,0);
    setDDCClock(bus, kIODDCLow);
    FB_END(setDDCClock,0,__LINE__,0);
    FB_START(setDDCData,kIODDCTristate,__LINE__,0);
    setDDCData(bus, kIODDCTristate);
    FB_END(setDDCData,0,__LINE__,0);

    for (i = 0 ; (kIOReturnSuccess == err) && (i < 8); i++)
    {
        // Wait for 1 msec and then pulse the clock
        IODelay( 100 );
        // Release the CLK line
        FB_START(setDDCClock,kIODDCTristate,__LINE__,0);
        setDDCClock(bus, kIODDCTristate);
        FB_END(setDDCClock,0,__LINE__,0);

        // Wait for a slow device by reading the SCL line until it is high
        // (A slow device will keep it low).  The DDC spec suggests a timeout
        // of 2ms here.
        // Setup for a timeout
        clock_interval_to_deadline(2, kMillisecondScale, &expirationTime);

        while ((0 == readDDCClock(bus)) && (kIOReturnSuccess == err))
        {
            AbsoluteTime_to_scalar(&now) = mach_absolute_time();
            if (CMP_ABSOLUTETIME(&now, &expirationTime) > 0)
                err = kIOReturnNotResponding;                   // Timed Out
        }

        // Read the data
        FB_START(readDDCData,bus,__LINE__,0);
        value = readDDCData(bus);
        FB_END(readDDCData,0,__LINE__,0);
        *data |= (value << (7-i));
        
        //we keep clock high for when sending bits....so do same here. Ensures display sees clock.
        // reach 100% success rate with NEC XE15
        
        IODelay( 200 );
        // Lower the clock line
        FB_START(setDDCClock,kIODDCLow,__LINE__,0);
        setDDCClock(bus, kIODDCLow);
        FB_END(setDDCClock,0,__LINE__,0);
        IODelay( 40 );
    }
    
    IOFB_END(i2cReadByte,err,0,0);
    return (err);
}               

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cWaitForBus()
//      Tristate DDC Clk and DDC Data lines

IOReturn IOFramebuffer::i2cWaitForBus(IOIndex bus, IOI2CBusTiming * timing)
{
    IOFB_START(i2cWaitForBus,bus,0,0);
    // should we wait for the bus here?
    
    FB_START(setDDCClock,kIODDCTristate,__LINE__,0);
    setDDCClock(bus, kIODDCTristate);
    FB_END(setDDCClock,0,__LINE__,0);
    FB_START(setDDCData,kIODDCTristate,__LINE__,0);
    setDDCData(bus, kIODDCTristate);
    FB_END(setDDCData,0,__LINE__,0);
    IODelay( 200 );
    
    IOFB_END(i2cWaitForBus,kIOReturnSuccess,0,0);
    return kIOReturnSuccess;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cReadDDCciData()
//
//      The parameters are described as follows:
//
//                      -> deviceAddress                                device's I2C address
//                      -> count                                                # of bytes to read
//                      <- buffer                                               buffer for the data
//

IOReturn IOFramebuffer::i2cReadDDCciData(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 count, UInt8 *buffer)
{
    IOFB_START(i2cReadDDCciData,bus,deviceAddress,count);
    // This is a funky call that encodes the length of the transaction in the response.
    // According to the VESA DDC/ci spec, the low 7 bits of second byte returned by the display 
    // will contain the length of the message less the checksum.  The card should then attempt to read 
    // that length plus the checksum but should not exceed "count" bytes.  
    // If the size exceeds "count", then the buffer should be filled with "count" bytes and the
    // transaction should be completed without copying more bytes into the buffer.
    
    IOReturn    err = kIOReturnSuccess;
    UInt32      i;
    UInt8       readLength;
    UInt32      bufferSize;
    Boolean     reportShortRead = false;
    
    // Assume that the bufferSize == count
    bufferSize = count;
    
    err = i2cWaitForBus(bus, timing);
    if( kIOReturnSuccess != err ) goto ErrorExit;
            
    i2cStart(bus, timing);
    
    i2cSendByte(bus, timing, deviceAddress | 0x01 );
            
    err = i2cWaitForAck(bus, timing);
    if( kIOReturnSuccess != err ) goto ErrorExit;
            
    for ( i = 0; i < bufferSize; i++ )
    {
        err = i2cReadByte(bus, timing, &buffer[i] );
        if( kIOReturnSuccess != err ) goto ErrorExit;
        
        i2cSendAck(bus, timing);
        
        if ( i == 1 )
        {
            // We have read the 2nd byte, so adjust the
            // bufferSize accordingly
            //
            readLength = buffer[i] & 0x07;
            if ( (readLength + 1) <= count )
            {
                // The read amount is less than our bufferSize, so
                // adjust that size
                //
                bufferSize = (readLength + 1);
            }
            else
            {
                // The amount to read  > than our bufferSize
                // so only read up to our buffer size and remember to
                // report that we didn't read all the data
                //
                reportShortRead = true;
            }
        }
    }

    
ErrorExit:
    i2cSendNack(bus, timing);
    i2cStop(bus, timing);
                    
    if ( reportShortRead )
            err = kIOReturnOverrun;
            
    IOFB_END(i2cReadDDCciData,err,0,0);
    return (err);

}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cRead()
//      Read a bunch of data via I2C
//
//      The parameters are described as follows:
//
//                      -> deviceAddress                device's I2C address
//                      -> numberOfBytes                number of bytes to read
//                      <- data                         the requested number of bytes of data

IOReturn IOFramebuffer::i2cRead(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 numberOfBytes, UInt8 * data)
{
    IOFB_START(i2cRead,deviceAddress,numberOfBytes,0);
    IOReturn    err = kIOReturnSuccess;
    int         i;
    
    err = i2cWaitForBus(bus, timing);
    if (kIOReturnSuccess != err)
        goto ErrorExit;
            
    i2cStart(bus, timing);
    
    i2cSendByte(bus, timing, deviceAddress | 0x01 );
            
    err = i2cWaitForAck(bus, timing);
    if (kIOReturnSuccess != err)
        goto ErrorExit;
            
    for (i = 0; i < numberOfBytes; i++)
    {
        data[i] = 0;
        err = i2cReadByte(bus, timing, &data[i] );
        if (kIOReturnSuccess != err)
            break;
        if (i != (numberOfBytes - 1))
            i2cSendAck(bus, timing);
    }
    
ErrorExit:
    i2cSendNack(bus, timing);
    i2cStop(bus, timing);
                    
    IOFB_END(i2cRead,err,0,0);
    return (err);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cWrite()
//      Write a bunch of data via I2C
//
//      The parameters are described as follows:
//
//                      -> deviceAddress                device's I2C address
//                      -> numberOfBytes                number of bytes to write
//                      -> data                         the number of bytes of data

IOReturn IOFramebuffer::i2cWrite(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 numberOfBytes, UInt8 * data)
{
    IOFB_START(i2cWrite,bus,deviceAddress,numberOfBytes);
    IOReturn    err = kIOReturnSuccess;
    UInt32      i;
    
    err = i2cWaitForBus(bus, timing);
    if (kIOReturnSuccess != err)
        goto ErrorExit;
            
    i2cStart(bus, timing);
    
    i2cSendByte(bus, timing, deviceAddress);
            
    err = i2cWaitForAck(bus, timing);
    if (kIOReturnSuccess != err)
        goto ErrorExit;
            
    for (i = 0; i < numberOfBytes; i++)
    {
        i2cSendByte(bus, timing, data[i] );
        err = i2cWaitForAck(bus, timing);
        if (kIOReturnSuccess != err)
            break;
    }

                    
ErrorExit:

    if (kIOReturnSuccess != err)
        i2cStop(bus, timing);
            
    IOFB_END(i2cWrite,err,0,0);
    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//
//      i2cSend9Stops()
//
//      Assume we are reading the DDC data, and display misses a few clocks, we send ack, display misses ack
//      The display might still be holding down the data line. Whenever we get an error, send nine stops.
//      If the display is waiting for a clock before going to the next bit, the stop will be interpreted
//      as a clock. It will go onto the next bit. Whenever it has finished writing the eigth bit, the
//      next stop will look like a stop....the display will release the bus.
//      8 bits, 9 stops. The display should see at least one stop....

void IOFramebuffer::i2cSend9Stops(IOIndex bus, IOI2CBusTiming * timing)
{
    IOFB_START(i2cSend9Stops,bus,0,0);
    for (UInt32 i = 0; i < 9; i++)
        i2cStop(bus, timing);
    IOFB_END(i2cSend9Stops,0,0,0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::setPreferences( IOService * props, OSDictionary * prefs )
{
    IOFB_START(setPreferences,0,0,0);
    if (!gIOFBPrefs)
    {
        prefs->retain();
        gIOFBPrefs = prefs;
        gIOFBPrefsParameters = OSDynamicCast(OSDictionary,
                                        props->getProperty(kIOGraphicsPrefsParametersKey));
        gIOFBIgnoreParameters = OSDynamicCast(OSDictionary,
                                        props->getProperty(kIOGraphicsIgnoreParametersKey));

    }
    IOFB_END(setPreferences,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

OSObject * IOFramebuffer::copyPreferences( void )
{
    IOFB_START(copyPreferences,0,0,0);
    if (gIOFBPrefsSerializer)
        gIOFBPrefsSerializer->retain();
    IOFB_END(copyPreferences,0,0,0);
    return (gIOFBPrefsSerializer);
}

OSObject * IOFramebuffer::copyPreference( IODisplay * display, const OSSymbol * key )
{
    IOFB_START(copyPreference,0,0,0);
    OSDictionary *   dict;
    OSObject *       value = 0;

    if (!gIOFBPrefs)
    {
        IOFB_END(copyPreference,-1,__LINE__,0);
        return (value);
    }

	if (!__private->displayPrefKey && display)
		__private->displayPrefKey = (const OSSymbol *) display->copyProperty(kIODisplayPrefKeyKey);
    if (!__private->displayPrefKey)
    {
        IOFB_END(copyPreference,-1,__LINE__,0);
        return (value);
    }

    if ((dict = OSDynamicCast(OSDictionary, gIOFBPrefs->getObject(__private->displayPrefKey))))
    {
        value = dict->getObject(key);
        if (value)
            value->retain();
    }

    IOFB_END(copyPreference,0,0,0);
    return (value);
}

bool IOFramebuffer::getIntegerPreference( IODisplay * display, const OSSymbol * key, UInt32 * value )
{
    IOFB_START(getIntegerPreference,0,0,0);
    bool found = false;
    OSObject *
    pref = copyPreference(display, key);
    if (pref)
    {
        OSNumber * num;
        if ((num = OSDynamicCast(OSNumber, pref)))
        {
            value[0] = num->unsigned32BitValue();
            DEBG1(thisName, " found(%s) %s = %d\n", __private->displayPrefKey->getCStringNoCopy(), 
            										key->getCStringNoCopy(), (uint32_t) value[0]);
            found = true;
        }
        pref->release();
    }
    IOFB_END(getIntegerPreference,found,0,0);
    return (found);
}

bool IOFramebuffer::setPreference( IODisplay * display, const OSSymbol * key, OSObject * value )
{
    IOFB_START(setPreference,0,0,0);
    OSDictionary *   dict;
    OSObject *       oldValue = 0;
    bool             madeChanges = false;

    if (!gIOFBPrefs)
    {
        IOFB_END(setPreference,false,__LINE__,0);
        return (false);
    }

	if (!__private->displayPrefKey && display)
		__private->displayPrefKey = (const OSSymbol *) display->copyProperty(kIODisplayPrefKeyKey);

	if (!__private->displayPrefKey)
    {
        IOFB_END(setPreference,false,__LINE__,0);
		return (false);
    }

    dict = OSDynamicCast(OSDictionary, gIOFBPrefs->getObject(__private->displayPrefKey));
    if (!dict)
    {
        dict = OSDictionary::withCapacity(4);
        if (dict)
        {
            gIOFBPrefs->setObject(__private->displayPrefKey, dict);
            dict->release();
            madeChanges = true;
        }
    }
    else if (key)
        oldValue = dict->getObject(key);

	if (dict 
	 && !gIOGraphicsPrefsVersionValue->isEqualTo(dict->getObject(gIOGraphicsPrefsVersionKey)))
	{
		dict->setObject(gIOGraphicsPrefsVersionKey, gIOGraphicsPrefsVersionValue);
		madeChanges = true;
	}
        
    if (key && dict)
    {
        if (!oldValue || (!oldValue->isEqualTo(value)))
        {
            dict->setObject(key, value);
            madeChanges = true;
        }
    }

    if (madeChanges)
    {
        DEBG(thisName, " sched prefs\n");
        gIOFBDelayedPrefsEvent->setTimeoutMS((UInt32) 2000);
    }

    IOFB_END(setPreference,true,0,0);
    return (true);
}

bool IOFramebuffer::setIntegerPreference( IODisplay * display, const OSSymbol * key, UInt32 value )
{
    IOFB_START(setIntegerPreference,value,0,0);
    bool ok = false;
    OSNumber *
    num = OSNumber::withNumber(value, 32);
    if (num)
    {
        ok = setPreference(display, key, num);
        num->release();
    }
    DEBG("", " %s = %d\n", key->getCStringNoCopy(), (uint32_t) value);
    IOFB_END(setIntegerPreference,ok,0,0);
    return (ok);
}

void IOFramebuffer::getTransformPrefs( IODisplay * display )
{
    IOFB_START(getTransformPrefs,0,0,0);
	OSObject * obj;
    UInt32     value;

	if ((obj = copyPreference(display, gIOFBStartupModeTimingKey)))
    {
		setProperty(gIOFBStartupModeTimingKey, obj);
        OSSafeReleaseNULL(obj);
    }

    if (getIntegerPreference(display, gIODisplayOverscanKey, &value))
    {
        if (value) 
             __private->selectedTransform  = __private->selectedTransform & ~kIOFBScalerUnderscan;
        else
            __private->selectedTransform = __private->selectedTransform | kIOFBScalerUnderscan;
    }

    if (__private->userSetTransform)
    {
        __private->userSetTransform = false;
        setIntegerPreference(display, gIOFBRotateKey, static_cast<UInt32>(__private->selectedTransform & ~kIOFBScalerUnderscan));
    }
    else if (getIntegerPreference(display, gIOFBRotateKey, &value))
        selectTransform(value, false);
    else if (__private->transform & ~kIOFBScalerUnderscan)
        selectTransform(0, false);
    IOFB_END(getTransformPrefs,0,0,0);
}



void IOFramebuffer::dpInterruptProc(OSObject * target, void * ref)
{
    IOFB_START(dpInterruptProc,0,0,0);
    IOFramebuffer * fb = (IOFramebuffer *) target;
    uintptr_t       delay = (uintptr_t) ref;

    if (delay && fb->__private->dpInterruptES)
        fb->__private->dpInterruptES->setTimeoutMS(static_cast<UInt32>(delay));
    IOFB_END(dpInterruptProc,0,0,0);
}

void IOFramebuffer::dpInterrupt(OSObject * owner, IOTimerEventSource * sender)
{
    IOFB_START(dpInterrupt,0,0,0);
    IOFramebuffer * fb = (IOFramebuffer *) owner;
    fb->dpProcessInterrupt();
    IOFB_END(dpInterrupt,0,0,0);
}

void IOFramebuffer::dpProcessInterrupt(void)
{
    IOFB_START(dpProcessInterrupt,0,0,0);
    IOReturn     err;
    IOI2CRequest request;
    UInt8        data[6];
    uintptr_t    bits, sel;

    if (__private->closed || !pagingState)
    {
        IOFB_END(dpProcessInterrupt,0,__LINE__,0);
        return;
    }

    sel = kIODPEventStart;
    FB_START(setAttributeForConnection,kConnectionHandleDisplayPortEvent,__LINE__,kIODPEventStart);
    err = setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);
    FB_END(setAttributeForConnection,err,__LINE__,0);

    bzero(&data[0], sizeof(data));
    do
    {
        bzero( &request, sizeof(request) );

        request.commFlags               = 0;
        request.sendAddress             = 0;
        request.sendTransactionType     = kIOI2CNoTransactionType;
        request.sendBuffer              = 0;
        request.sendBytes               = 0;

        request.replyAddress            = kDPRegisterLinkStatus;
        request.replyTransactionType    = kIOI2CDisplayPortNativeTransactionType;
        request.replyBuffer             = (vm_address_t) &data[0];
        request.replyBytes              = sizeof(data);

        FB_START(doI2CRequest,__private->dpBusID,__LINE__,0);
        err = doI2CRequest(__private->dpBusID, 0, &request);
        FB_END(doI2CRequest,err,__LINE__,0);
        if( (kIOReturnSuccess != err) || (kIOReturnSuccess != request.result))
            break;

        bits = data[1];
        if (!bits)
            break;
        DEBG1(thisName, "dp events: 0x%02lx\n", bits);

        if (kDPIRQRemoteControlCommandPending & bits)
        {
            sel = kIODPEventRemoteControlCommandPending;
            FB_START(setAttributeForConnection,kConnectionHandleDisplayPortEvent,__LINE__,kIODPEventRemoteControlCommandPending);
            err = setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);
            FB_END(setAttributeForConnection,err,__LINE__,0);
        }
        if (kDPIRQAutomatedTestRequest & bits)
        {
            sel = kIODPEventAutomatedTestRequest;
            FB_START(setAttributeForConnection,kConnectionHandleDisplayPortEvent,__LINE__,kIODPEventAutomatedTestRequest);
            err = setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);
            FB_END(setAttributeForConnection,err,__LINE__,0);
        }
        if (kDPIRQContentProtection & bits)
        {
            sel = kIODPEventContentProtection;
            FB_START(setAttributeForConnection,kConnectionHandleDisplayPortEvent,__LINE__,kIODPEventContentProtection);
            err = setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);
            FB_END(setAttributeForConnection,err,__LINE__,0);
        }
        if (kDPIRQMCCS & bits)
        {
            sel = kIODPEventMCCS;
            FB_START(setAttributeForConnection,kConnectionHandleDisplayPortEvent,__LINE__,kIODPEventMCCS);
            err = setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);
            FB_END(setAttributeForConnection,err,__LINE__,0);

            IOFBInterruptProc proc;
            if ((proc = __private->interruptRegisters[kIOFBMCCSInterruptRegister].handler)
                && __private->interruptRegisters[kIOFBMCCSInterruptRegister].state)
            {
                (*proc)(__private->interruptRegisters[kIOFBMCCSInterruptRegister].target,
                        __private->interruptRegisters[kIOFBMCCSInterruptRegister].ref);
            }
        }
        if (kDPIRQSinkSpecific & bits)
        {
            sel = kIODPEventSinkSpecific;
            FB_START(setAttributeForConnection,kConnectionHandleDisplayPortEvent,__LINE__,kIODPEventSinkSpecific);
            err = setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);
            FB_END(setAttributeForConnection,err,__LINE__,0);
        }

        request.sendAddress             = kDPRegisterServiceIRQ;
        request.sendTransactionType     = kIOI2CDisplayPortNativeTransactionType;
        request.sendBuffer              = (vm_address_t) &data[1];
        request.sendBytes               = sizeof(data[1]);

        request.replyAddress            = kDPRegisterLinkStatus;
        request.replyTransactionType    = kIOI2CDisplayPortNativeTransactionType;
        request.replyBuffer             = (vm_address_t) &data[0];
        request.replyBytes              = sizeof(data);

        FB_START(doI2CRequest,__private->dpBusID,__LINE__,0);
        err = doI2CRequest(__private->dpBusID, 0, &request);
        FB_END(doI2CRequest,err,__LINE__,0);
        if( (kIOReturnSuccess != err) || (kIOReturnSuccess != request.result))
            break;

        if (data[1] == bits)
        {
            DEBG(thisName, "dp events not cleared: 0x%02x\n", data[1]);
            break;
        }
    }
    while (false);

    sel = kIODPEventIdle;
    FB_START(setAttributeForConnection,kConnectionHandleDisplayPortEvent,__LINE__,kIODPEventIdle);
    err = setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);
    FB_END(setAttributeForConnection,err,__LINE__,0);

    DEBG(thisName, "dp sinkCount %d\n", (kDPLinkStatusSinkCountMask & data[0]));

    if (__private->dpDongle)
    {
        UInt8 sinkCount = (kDPLinkStatusSinkCountMask & data[0]);
        if (sinkCount != __private->dpDongleSinkCount) do
        {
            __private->dpDongleSinkCount = sinkCount;
            if (captured)
                continue;
            triggerEvent(kIOFBEventProbeAll);
            DEBG(thisName, "dp dongle hpd probeDP\n");
        }
        while (false);
    }

    IOFB_END(dpProcessInterrupt,0,0,0);
    return;
}

void IOFramebuffer::dpUpdateConnect(void)
{
    OSObject * obj;
    OSData *   data;

    __private->dpDongle          = false;
    if (getProvider()->getProperty(kIOFBDPDeviceIDKey)
        && (obj = getProvider()->copyProperty(kIOFBDPDeviceTypeKey)))
    {
        data = OSDynamicCast(OSData, obj);
        __private->dpDongle =
        (data && data->isEqualTo(kIOFBDPDeviceTypeDongleKey, static_cast<unsigned int>(strlen(kIOFBDPDeviceTypeDongleKey))));
        obj->release();

        if (__private->dpDongle)
        {
            IOReturn     err;
            IOI2CRequest request;
            UInt8        tdata[6];

            bzero(&tdata[0], sizeof(tdata));
            do
            {
                bzero( &request, sizeof(request) );

                request.commFlags               = 0;
                request.sendAddress             = 0;
                request.sendTransactionType     = kIOI2CNoTransactionType;
                request.sendBuffer              = 0;
                request.sendBytes               = 0;

                request.replyAddress            = kDPRegisterLinkStatus;
                request.replyTransactionType    = kIOI2CDisplayPortNativeTransactionType;
                request.replyBuffer             = (vm_address_t) &tdata[0];
                request.replyBytes              = sizeof(tdata);

                FB_START(doI2CRequest,__private->dpBusID,__LINE__,0);
                err = doI2CRequest(__private->dpBusID, 0, &request);
                FB_END(doI2CRequest,err,__LINE__,0);
                if( (kIOReturnSuccess != err) || (kIOReturnSuccess != request.result))
                    break;

                __private->dpDongleSinkCount = (kDPLinkStatusSinkCountMask & tdata[0]);
            }
            while (false);
        }
    }
    DEBG(thisName, "dp dongle %d, sinks %d\n", __private->dpDongle, __private->dpDongleSinkCount);
}


#pragma mark -

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IODisplayParameterHandler

OSDefineMetaClassAndStructors(IOFramebufferParameterHandler, IODisplayParameterHandler)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOFramebufferParameterHandler * IOFramebufferParameterHandler::withFramebuffer( IOFramebuffer * framebuffer )
{
    IOFBPH_START(withFramebuffer,0,0,0);
    IOFramebufferParameterHandler * handler;
    uintptr_t                       count = 0;

    FB_START(getAttributeForConnection,kConnectionDisplayParameterCount,__LINE__,0);
    IOReturn err = framebuffer->getAttributeForConnection( 0, kConnectionDisplayParameterCount, &count);
    FB_END(getAttributeForConnection,err,__LINE__,0);
    if (kIOReturnSuccess != err)
    {
        IOFBPH_END(withFramebuffer,-1,0,0);
        return (NULL);
    }


    handler = new IOFramebufferParameterHandler;
    if (handler && !handler->init())
    {
        handler->release();
        handler = 0;
    }
    if (handler)
        handler->fFramebuffer = framebuffer;

    IOFBPH_END(withFramebuffer,0,0,0);
    return (handler);
}

void IOFramebufferParameterHandler::free()
{
    IOFBPH_START(free,0,0,0);
    if (fDisplayParams)
        fDisplayParams->release();

    super::free();
    IOFBPH_END(free,0,0,0);
}

bool IOFramebufferParameterHandler::setDisplay( IODisplay * display )
{
    IOFBPH_START(setDisplay,0,0,0);
    fDisplay = display;

    fFramebuffer->setPreference(display, 0, 0);         // register display

    if (!display)
    {
        IOFBPH_END(setDisplay,false,0,0);
        return (false);
    }

    fFramebuffer->getTransformPrefs(display);

    IOFBPH_END(setDisplay,true,0,0);
    return (true);
}

void IOFramebufferParameterHandler::displayModeChange( void )
{
    IOFBPH_START(displayModeChange,0,0,0);
    IODisplay *      display = fDisplay;
    IOReturn         ret;
    uintptr_t        count = 0;
    UInt32           str[2];
    const OSSymbol * sym;
    uintptr_t        value[16];
    uintptr_t *      attributes;
    OSDictionary *   allParams;
    OSDictionary *   newDict = 0;
    OSDictionary *   oldParams;
    const OSSymbol * key;
    OSIterator *     iter;

    if (!display)
    {
        IOFBPH_END(displayModeChange,-1,0,0);
        return;
    }

    OSObject *paramProp = display->copyProperty(gIODisplayParametersKey);
    allParams = OSDynamicCast(OSDictionary, paramProp);
    if (allParams)
        newDict = OSDictionary::withDictionary(allParams);
    OSSafeReleaseNULL(paramProp);

    ret = fFramebuffer->getAttributeForConnectionParam(
                            0, kConnectionDisplayParameterCount, &count);
    if (kIOReturnSuccess != ret)
        count = 0;

    DEBG(fFramebuffer->thisName, " (%x) count %ld\n", ret, count);

    oldParams = fDisplayParams;
    do
    {
        if (count)
            fDisplayParams = OSDictionary::withCapacity(static_cast<unsigned int>(count));
        else
            fDisplayParams = 0;
        if (!fDisplayParams)
            continue;

        attributes = IONew(uintptr_t, count);
        if (!attributes)
            continue;
    
        FB_START(getAttributeForConnection,kConnectionDisplayParameters,__LINE__,0);
        IOReturn err = fFramebuffer->getAttributeForConnection( 0, kConnectionDisplayParameters, attributes);
        FB_END(getAttributeForConnection,err,__LINE__,0);
        if (kIOReturnSuccess != err)
            continue;
    
        str[1] = 0;
        for (uint32_t i = 0; i < count; i++)
        {
            DEBG1(fFramebuffer->thisName, " [%d] 0x%08lx '%c%c%c%c'\n", i, attributes[i], FEAT(attributes[i]));

            if (attributes[i] < 0x00ffffff)
                continue;

            OSWriteBigInt32(str, 0, static_cast<UInt32>(attributes[i]));
            sym = OSSymbol::withCString((const char *) str);
            if (!sym)
                continue;

			if ((!gIOFBIgnoreParameters || !gIOFBIgnoreParameters->getObject(sym))
			 && (kIOReturnSuccess == fFramebuffer->getAttributeForConnectionParam(0, static_cast<IOSelect>(attributes[i]), &value[0])))
            {
            	OSObject * obj;

				DEBG1(fFramebuffer->thisName, " [%d] drvr  %s = %ld, (%ld - %ld)\n", i, (const char *) str, value[0], value[1], value[2]);

                if (gIOFBPrefsParameters && (obj = gIOFBPrefsParameters->getObject(sym)))
                {
					OSNumber * prefDefault = NULL;
                    UInt32     pref;

                    if ((fFramebuffer->getIntegerPreference(display, sym, &pref))
					 || (prefDefault = OSDynamicCast(OSNumber, obj)))
					{
						if (prefDefault)
							pref = prefDefault->unsigned32BitValue();
						if (pref < value[1])
							pref = static_cast<UInt32>(value[1]);
						if (pref > value[2])
							pref = static_cast<UInt32>(value[2]);
						value[0] = pref;
					}
                }
				if (kConnectionColorMode == attributes[i])
				{
					IODisplay::addParameter(fDisplayParams, gIODisplaySelectedColorModeKey, 0, kIODisplayColorModeRGBLimited);
					IODisplay::setParameter(fDisplayParams, gIODisplaySelectedColorModeKey, kIODisplayColorModeRGB);
				}
                IODisplay::addParameter(fDisplayParams, sym, static_cast<SInt32>(value[1]), static_cast<SInt32>(value[2]));
				IODisplay::setParameter(fDisplayParams, sym, static_cast<SInt32>(value[0]));
            }
            DEBG1(fFramebuffer->thisName, " [%d] added %s = %ld, (%ld - %ld)\n", i, (const char *) str, value[0], value[1], value[2]);
            sym->release();
        }
    
        IODelete(attributes, uintptr_t, count);
    }
    while (false);

    if (oldParams)
    {
        if (newDict)
        {
            iter = OSCollectionIterator::withCollection(oldParams);
            if (iter)
            {
                while ((key = (const OSSymbol *) iter->getNextObject()))
                {
                    if (!fDisplayParams || !fDisplayParams->getObject(key))
                        newDict->removeObject(key);
                }
                iter->release();
            }
        }
        oldParams->release();
    }

    if (newDict)
    {
        if (fDisplayParams)
            newDict->merge(fDisplayParams);
        display->setProperty(gIODisplayParametersKey, newDict);
        newDict->release();
    }
    else if (fDisplayParams)
        display->setProperty(gIODisplayParametersKey, fDisplayParams);

    IOFBPH_END(displayModeChange,0,0,0);
}

bool IOFramebufferParameterHandler::doIntegerSet( OSDictionary * params,
                               const OSSymbol * paramName, UInt32 value )
{
    IOFBPH_START(doIntegerSet,value,0,0);
    UInt32      attribute;
    bool        ok;
    SInt32      min, max;

    fFramebuffer->fbLock();

    if (fDisplayParams && fDisplayParams->getObject(paramName))
    {
        if (fDisplay 
        	&& gIOFBPrefsParameters
        	&& gIOFBPrefsParameters->getObject(paramName)
			&& (params = IODisplay::getIntegerRange(fDisplayParams, paramName, 0, &min, &max))
        	&& (min != max))
		{
            fFramebuffer->setIntegerPreference(fDisplay, paramName, value);
		}

        attribute = OSReadBigInt32(paramName->getCStringNoCopy(), 0);

		if (kConnectionColorMode == attribute)
		{
			if ((kIODisplayColorModeAuto == value)
				&& (AUTO_COLOR_MODE != kIODisplayColorModeRGB) 
				&& fFramebuffer->__private->colorModesAllowed
				&& (AUTO_COLOR_MODE & fFramebuffer->__private->colorModesSupported))
			{
				value = AUTO_COLOR_MODE;
			}
			else
			{
				value = kIODisplayColorModeRGB;
			}
			IODisplay::setParameter(fDisplayParams, gIODisplaySelectedColorModeKey, value);
		}
    
        ok = (kIOReturnSuccess == fFramebuffer->setAttributeForConnectionParam(
                                        0, attribute, value));

		DEBG1(fFramebuffer->thisName, "(%d) %s = %d\n", ok, paramName->getCStringNoCopy(), (int) value);
    }
    else
        ok = false;

    
    bool bIsWSAAActive = false;
    if (fDisplay)
        bIsWSAAActive = (kIOWSAA_DeferEnd != fDisplay->fWSAADeferState) ? true : false;

    if ((gIODisplayParametersFlushKey == paramName) && (false == bIsWSAAActive))
    {
	    fFramebuffer->setAttributeForConnectionParam(0, kConnectionFlushParameters, true);
	}

    fFramebuffer->fbUnlock();

    IOFBPH_END(doIntegerSet,ok,0,0);
    return (ok);
}

bool IOFramebufferParameterHandler::doDataSet( const OSSymbol * paramName, OSData * value )
{
    IOFBPH_START(doDataSet,0,0,0);
    IOFBPH_END(doDataSet,false,0,0);
    return (false);
}

bool IOFramebufferParameterHandler::doUpdate( void )
{
    IOFBPH_START(doUpdate,0,0,0);
    bool ok = true;
    IOFBPH_END(doUpdate,ok,0,0);
    return (ok);
}
#pragma mark -

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOI2CInterface

OSDefineMetaClassAndStructors(IOFramebufferI2CInterface, IOI2CInterface)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOFramebufferI2CInterface * IOFramebufferI2CInterface::withFramebuffer(
    IOFramebuffer * framebuffer, OSDictionary * info )
{
    IOFBI2C_START(withFramebuffer,0,0,0);
    IOFramebufferI2CInterface * interface;

    interface = new IOFramebufferI2CInterface;
    info = OSDictionary::withDictionary(info);

    if (interface && info)
    {
        interface->fFramebuffer = framebuffer;
        if (!interface->init(info)
        ||  !interface->attach(framebuffer)
        ||  !interface->start(framebuffer))
        {
            interface->detach( framebuffer );
            interface->release();
            interface = 0;
        }
    }
    if (info)
        info->release();

    IOFBI2C_END(withFramebuffer,0,0,0);
    return (interface);
}

bool IOFramebufferI2CInterface::start( IOService * provider )
{
    IOFBI2C_START(start,0,0,0);
    bool       ok = false;
    OSNumber * num;

    if (!super::start(provider))
    {
        IOFBI2C_END(start,false,__LINE__,0);
        return (false);
    }

    do
    {
        num = OSDynamicCast(OSNumber, getProperty(kIOI2CInterfaceIDKey));
        if (!num)
            break;
        fBusID = num->unsigned32BitValue();

        num = OSDynamicCast(OSNumber, getProperty(kIOI2CBusTypeKey));
        if (!num)
            setProperty(kIOI2CBusTypeKey, (UInt64) kIOI2CBusTypeI2C, 32);

        num = OSDynamicCast(OSNumber, getProperty(kIOI2CTransactionTypesKey));
        if (num)
            fSupportedTypes = num->unsigned32BitValue();
        else
        {
            fSupportedTypes = ((1 << kIOI2CNoTransactionType)
                             | (1 << kIOI2CSimpleTransactionType)
                             | (1 << kIOI2CDDCciReplyTransactionType)
                             | (1 << kIOI2CCombinedTransactionType));
            setProperty(kIOI2CTransactionTypesKey, (UInt64) fSupportedTypes, 32);
        }

        num = OSDynamicCast(OSNumber, getProperty(kIOI2CSupportedCommFlagsKey));
        if (num)
            fSupportedCommFlags = num->unsigned32BitValue();
        else
        {
            fSupportedCommFlags = kIOI2CUseSubAddressCommFlag;
            setProperty(kIOI2CSupportedCommFlagsKey, (UInt64) fSupportedCommFlags, 32);
        }

        num = OSDynamicCast(OSNumber, fFramebuffer->getProperty(kIOFramebufferOpenGLIndexKey));
        UInt64 id = fBusID;
        if (num) id |= (num->unsigned64BitValue() << 32);
        registerI2C(id);

        ok = true;
    }
    while (false);

    IOFBI2C_END(start,ok,0,0);
    return (ok);
}

IOReturn IOFramebufferI2CInterface::startIO( IOI2CRequest * request )
{
    IOFBI2C_START(startIO,0,0,0);
    IOReturn            err;

    fFramebuffer->fbLock();

    if (fFramebuffer->isInactive() || !fFramebuffer->isPowered()) {
        fFramebuffer->fbUnlock();
        IOFBI2C_END(startIO,kIOReturnOffline,0,0);
        return kIOReturnOffline;
    }

    do
    {
        if (0 == ((1 << request->sendTransactionType) & fSupportedTypes))
        {
            err = kIOReturnUnsupportedMode;
            continue;
        }
        if (0 == ((1 << request->replyTransactionType) & fSupportedTypes))
        {
            err = kIOReturnUnsupportedMode;
            continue;
        }
        if (request->commFlags != (request->commFlags & fSupportedCommFlags))
        {
            err = kIOReturnUnsupportedMode;
            continue;
        }

        FB_START(doI2CRequest,fBusID,__LINE__,0);
        err = fFramebuffer->doI2CRequest( fBusID, 0, request );
        FB_END(doI2CRequest,err,__LINE__,0);
    }
    while (false);

    if (kIOReturnSuccess != err)
    {
        request->result = err;
        if (request->completion)
            (*request->completion)(request);

        err = kIOReturnSuccess;
    }

    fFramebuffer->fbUnlock();

    IOFBI2C_END(startIO,err,0,0);
    return (err);
}

bool IOFramebufferI2CInterface::willTerminate(IOService *provider, IOOptionBits options)
{
    IOFBI2C_START(willTerminate,options,0,0);
    DEBG1(fName, " provider=%p options=%#x\n", provider, (uint32_t)options);
    bool status = super::willTerminate(provider, options);
    IOFBI2C_END(willTerminate,status,0,0);
    return status;
}

bool IOFramebufferI2CInterface::didTerminate(IOService *provider, IOOptionBits options, bool *defer)
{
    IOFBI2C_START(didTerminate,options,0,0);
    DEBG1(fName, " provider=%p options=%#x\n", provider, (uint32_t)options);
    bool status = super::didTerminate(provider, options, defer);
    IOFBI2C_END(didTerminate,status,0,0);
    return status;
}

bool IOFramebufferI2CInterface::requestTerminate(IOService *provider, IOOptionBits options)
{
    IOFBI2C_START(requestTerminate,options,0,0);
    DEBG1(fName, " provider=%p options=%#x\n", provider, (uint32_t)options);
    bool status = super::requestTerminate(provider, options);
    IOFBI2C_END(requestTerminate,status,0,0);
    return status;
}

bool IOFramebufferI2CInterface::terminate(IOOptionBits options)
{
    IOFBI2C_START(terminate,0,0,0);
    DEBG1(fName, "\n");
    bool status = super::terminate(options);
    IOFBI2C_END(terminate,status,0,0);
    return status;
}

bool IOFramebufferI2CInterface::finalize(IOOptionBits options)
{
    IOFBI2C_START(finalize,0,0,0);
    DEBG1(fName, "(%#x)\n", options);
    bool status = super::finalize(options);
    IOFBI2C_END(finalize,status,0,0);
    return status;
}

void IOFramebufferI2CInterface::stop(IOService *provider)
{
    IOFBI2C_START(stop,0,0,0);
    DEBG1(fName, "(%p)\n", provider);
    super::stop(provider);
    IOFBI2C_END(stop,0,0,0);
}

void IOFramebufferI2CInterface::free()
{
    IOFBI2C_START(free,0,0,0);
    DEBG1(fName, "\n");
    super::free();
    IOFBI2C_END(free,0,0,0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebufferI2CInterface::create( IOFramebuffer * framebuffer, const char *fbName )
{
    IOFBI2C_START(create,0,0,0);
    IOReturn                    err = kIOReturnSuccess;
    IOFramebufferI2CInterface * interface;
    UInt32                      idx;
    OSArray *                   busArray;
    OSArray *                   interfaceIDArray;
    OSDictionary *              dict;
    OSObject *                  num;
    bool                        ok = true;

    interfaceIDArray = OSArray::withCapacity(1);
    if (!interfaceIDArray)
    {
        IOFBI2C_END(create,kIOReturnNoMemory,0,0);
        return (kIOReturnNoMemory);
    }

    busArray = OSDynamicCast(OSArray, framebuffer->getProperty(kIOFBI2CInterfaceInfoKey));
    do
    {
        if (!busArray)
            continue;
        for (idx = 0; (dict = OSDynamicCast(OSDictionary, busArray->getObject(idx))); idx++)
        {
            interface = IOFramebufferI2CInterface::withFramebuffer(framebuffer, dict);
            if (!interface)
                break;
#if RLOG
            snprintf(interface->fName, sizeof(interface->fName), "%s-I2C", fbName);
#endif
            num = interface->getProperty(kIOI2CInterfaceIDKey);
            OSSafeReleaseNULL(interface);
            if (num)
                interfaceIDArray->setObject(num);
            else
                break;
        }

        ok = (idx == busArray->getCount());
    }
    while (false);

    if (ok && interfaceIDArray->getCount())
        framebuffer->setProperty(kIOFBI2CInterfaceIDsKey, interfaceIDArray);

    interfaceIDArray->release();

    IOFBI2C_END(create,err,0,0);
    return (err);
}
#pragma mark -

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSMetaClassDefineReservedUsed(IOFramebuffer, 0);
OSMetaClassDefineReservedUsed(IOFramebuffer, 1);
OSMetaClassDefineReservedUsed(IOFramebuffer, 2);

OSMetaClassDefineReservedUnused(IOFramebuffer, 3);
OSMetaClassDefineReservedUnused(IOFramebuffer, 4);
OSMetaClassDefineReservedUnused(IOFramebuffer, 5);
OSMetaClassDefineReservedUnused(IOFramebuffer, 6);
OSMetaClassDefineReservedUnused(IOFramebuffer, 7);
OSMetaClassDefineReservedUnused(IOFramebuffer, 8);
OSMetaClassDefineReservedUnused(IOFramebuffer, 9);
OSMetaClassDefineReservedUnused(IOFramebuffer, 10);
OSMetaClassDefineReservedUnused(IOFramebuffer, 11);
OSMetaClassDefineReservedUnused(IOFramebuffer, 12);
OSMetaClassDefineReservedUnused(IOFramebuffer, 13);
OSMetaClassDefineReservedUnused(IOFramebuffer, 14);
OSMetaClassDefineReservedUnused(IOFramebuffer, 15);
OSMetaClassDefineReservedUnused(IOFramebuffer, 16);
OSMetaClassDefineReservedUnused(IOFramebuffer, 17);
OSMetaClassDefineReservedUnused(IOFramebuffer, 18);
OSMetaClassDefineReservedUnused(IOFramebuffer, 19);
OSMetaClassDefineReservedUnused(IOFramebuffer, 20);
OSMetaClassDefineReservedUnused(IOFramebuffer, 21);
OSMetaClassDefineReservedUnused(IOFramebuffer, 22);
OSMetaClassDefineReservedUnused(IOFramebuffer, 23);
OSMetaClassDefineReservedUnused(IOFramebuffer, 24);
OSMetaClassDefineReservedUnused(IOFramebuffer, 25);
OSMetaClassDefineReservedUnused(IOFramebuffer, 26);
OSMetaClassDefineReservedUnused(IOFramebuffer, 27);
OSMetaClassDefineReservedUnused(IOFramebuffer, 28);
OSMetaClassDefineReservedUnused(IOFramebuffer, 29);
OSMetaClassDefineReservedUnused(IOFramebuffer, 30);
OSMetaClassDefineReservedUnused(IOFramebuffer, 31);
