#include "PDPowerVR.h"
#include "pvr_port.h"
#include "pvr_gem.h"
extern "C" {
#include "pvr_vm.h"
#include "pvr_queue.h"
}

#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>
#include <stdarg.h>

#define super IOService
OSDefineMetaClassAndStructors(PDPowerVR, IOService);

// the bsp driver's insmod sequence, read back from the board: gpu_top domain, pll-gpu, gpu0 from
// pll-gpu /1, bus gate, reset deassert. gpu_core powers on by itself once the core leaves reset

#define kA733GPUPhys		0x01800000ULL
#define kA733CCUPhys		0x02002000ULL
#define kA733PCKPhys		0x07060000ULL

// ccu: pll-gpu control, gpu0 module clock, gpu0 bus gate and reset, the critical ahb and mbus gates
#define kCCU_PLL_GPU		0x0e0
#define kPLL_ENABLE		(1u << 31)
#define kPLL_LDO_EN		(1u << 30)
#define kPLL_LOCK_EN		(1u << 29)
#define kPLL_LOCK		(1u << 28)
#define kPLL_OUTPUT		(1u << 27)
#define kPLL_CTRL_BITS		0xf8000000u
// n = 70, m = 2 from the 24 mhz reference: 840 mhz, the factors linux keeps
#define kPLL_GPU_FACTORS	0x01104500u
#define kCCU_GPU0_CLK		0xb20
#define kGPU0_CLK_ENABLE	(1u << 31)
#define kGPU0_CLK_UPD_KEY	(1u << 27)
#define kGPU0_CLK_SRC_PLL_GPU	(0u << 24)
#define kGPU0_CLK_DIV1		0u
#define kCCU_GPU0_BGR		0xb24
#define kGPU0_BGR_GATE		(1u << 0)
#define kGPU0_BGR_RESET		(1u << 16)
#define kCCU_AHB_GATE		0x5c0
#define kAHB_GPU0		(1u << 7)
#define kCCU_MBUS_GATE		0x5e0
#define kMBUS_GPU0		(1u << 16)

// pck-600 ppu per domain at domain << 12: policy, status, and the delays linux programs
#define kPCK_DOMAIN(d)		((d) << 12)
#define kPPU_PWPR		0x000
#define kPPU_PWSR		0x008
#define kPPU_DEV_CTRL0_DELAY	0x170
#define kPPU_DEV_CTRL1_DELAY	0x174
#define kPPU_LOGIC_SW0_DELAY	0xc00
#define kPPU_LOGIC_SW1_DELAY	0xc04
#define kPPU_OFF2ON_DELAY	0xc10
#define kPPU_POLICY_MASK	0xfu
#define kPPU_ON			0x8u
#define kPCK_GPU_TOP		5
#define kPCK_GPU_CORE		6
#define kPCKMapSize		0x7000

extern "C" const uint8_t *pd_pvr_fw_blob(uint32_t *size);

static uint64_t
pd_now_us(void)
{
	uint64_t ns;

	absolutetime_to_nanoseconds(mach_absolute_time(), &ns);
	return ns / 1000;
}

static volatile uint8_t *
map_phys(IOPhysicalAddress phys, IOByteCount size, IOOptionBits cache, IOMemoryMap **outMap)
{
	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(phys, size, kIODirectionOutIn);
	IOMemoryMap *map;

	if (desc == NULL)
		return NULL;
	map = desc->map(kIOMapAnywhere | cache);
	desc->release();
	if (map == NULL)
		return NULL;
	*outMap = map;
	return (volatile uint8_t *)map->getVirtualAddress();
}

extern "C" void
pd_pvr_log(const char *fmt, ...)
{
	va_list ap;
	char buf[256];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	IOLog("PDPowerVR: %s", buf);
}

extern "C" int
pd_pvr_slot_diag(void)
{
	static int state = -1;
	int on = 0;

	if (state < 0) state = PE_parse_boot_argn("pdpvrslots", &on, sizeof(on)) && on;
	return state;
}

extern "C" void *
pd_pvr_lock_alloc(void)
{
	return IOLockAlloc();
}

extern "C" void
pd_pvr_lock(void *lock)
{
	IOLockLock((IOLock *)lock);
}

extern "C" void
pd_pvr_unlock(void *lock)
{
	IOLockUnlock((IOLock *)lock);
}

// one gpu, so one event: the interrupt bumps the generation and wakes every waiter
static IOLock *gWaitLock;
static uint32_t gWaitGen;

extern "C" void
pd_pvr_wait(struct pvr_device *, u32 timeout_ms)
{
	uint64_t deadline;
	uint32_t gen;

	if (gWaitLock == NULL) {
		IOSleep(timeout_ms);
		return;
	}
	clock_interval_to_deadline(timeout_ms, kMillisecondScale, &deadline);
	IOLockLock(gWaitLock);
	gen = gWaitGen;
	while (gen == gWaitGen) {
		if (IOLockSleepDeadline(gWaitLock, &gWaitGen, *(AbsoluteTime *)&deadline, THREAD_UNINT) == THREAD_TIMED_OUT)
			break;
	}
	IOLockUnlock(gWaitLock);
}

extern "C" u32
pd_pvr_wait_gen(void)
{
	return __atomic_load_n(&gWaitGen, __ATOMIC_ACQUIRE);
}

extern "C" void
pd_pvr_wait_since(struct pvr_device *, u32 gen, u32 timeout_ms)
{
	uint64_t deadline;

	if (gWaitLock == NULL) {
		IOSleep(timeout_ms);
		return;
	}
	clock_interval_to_deadline(timeout_ms, kMillisecondScale, &deadline);
	IOLockLock(gWaitLock);
	while (gen == gWaitGen) {
		if (IOLockSleepDeadline(gWaitLock, &gWaitGen, *(AbsoluteTime *)&deadline, THREAD_UNINT) == THREAD_TIMED_OUT)
			break;
	}
	IOLockUnlock(gWaitLock);
}

extern "C" void
pd_pvr_wake(struct pvr_device *)
{
	if (gWaitLock == NULL)
		return;
	IOLockLock(gWaitLock);
	gWaitGen++;
	IOLockWakeup(gWaitLock, &gWaitGen, false);
	IOLockUnlock(gWaitLock);
}

extern "C" void
pd_pvr_udelay(u32 us)
{
	IODelay(us);
}

extern "C" u64
pd_pvr_now_us(void)
{
	return pd_now_us();
}

// firmware memory: contiguous pages, used only through a write-combined mapping of their
// physical address. the buffer's own cached alias is cleaned first so no dirty line lands later
extern "C" int
pd_pvr_alloc(u32 size, u64 *phys, u8 **cpu, void **bmdOut, void **mapOut)
{
	IOBufferMemoryDescriptor *bmd;
	IOMemoryMap *map = NULL;
	IOByteCount seg = 0;
	uint8_t *cached;
	volatile uint64_t *wc;

	bmd = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
	    kIODirectionInOut | kIOMemoryPhysicallyContiguous, size, 0x0000000ffffff000ULL);
	if (bmd == NULL)
		return -ENOMEM;
	if (bmd->prepare() != kIOReturnSuccess) {
		bmd->release();
		return -ENOMEM;
	}
	*phys = bmd->getPhysicalSegment(0, &seg, kIOMemoryMapperNone);
	if (*phys == 0 || seg < size) {
		pd_pvr_log("buffer of 0x%x bytes is not contiguous\n", size);
		bmd->complete();
		bmd->release();
		return -ENOMEM;
	}

	cached = (uint8_t *)bmd->getBytesNoCopy();
	for (u32 off = 0; off < size; off += 64) {
		__asm__ volatile ("dc civac, %0" :: "r"(cached + off) : "memory");
	}
	__asm__ volatile ("dsb sy" ::: "memory");

	if (map_phys(*phys, size, kIOMapWriteCombineCache, &map) == NULL) {
		pd_pvr_log("cannot map 0x%llx write-combined\n", *phys);
		bmd->complete();
		bmd->release();
		return -ENOMEM;
	}
	wc = (volatile uint64_t *)map->getVirtualAddress();
	for (u32 i = 0; i < size / 8; i++) {
		wc[i] = 0;
	}
	__asm__ volatile ("dsb sy" ::: "memory");

	*cpu = (u8 *)map->getVirtualAddress();
	*bmdOut = bmd;
	*mapOut = map;
	return 0;
}

extern "C" void
pd_pvr_free(void *bmdIn, void *mapIn)
{
	IOBufferMemoryDescriptor *bmd = (IOBufferMemoryDescriptor *)bmdIn;
	IOMemoryMap *map = (IOMemoryMap *)mapIn;

	if (map != NULL)
		map->release();
	if (bmd != NULL) {
		bmd->complete();
		bmd->release();
	}
}

bool
PDPowerVR::start(IOService *provider)
{
	uint32_t enable = 1;
	thread_t th = NULL;

	if (!super::start(provider))
		return false;
	// pdpvr=0 leaves the gpu alone
	if (PE_parse_boot_argn("pdpvr", &enable, sizeof(enable)) && enable == 0) {
		IOLog("PDPowerVR: disabled by pdpvr=0\n");
		return false;
	}

	fRegMap = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
	if (fRegMap == NULL || fRegMap->getPhysicalAddress() != kA733GPUPhys) {
		IOLog("PDPowerVR: no gpu registers at 0x%llx\n", kA733GPUPhys);
		return false;
	}
	fRegs = (volatile uint8_t *)fRegMap->getVirtualAddress();
	fRegsPhys = fRegMap->getPhysicalAddress();
	fCCU = map_phys(kA733CCUPhys, 0x1000, kIOMapInhibitCache, &fCCUMap);
	fPCK = map_phys(kA733PCKPhys, kPCKMapSize, kIOMapInhibitCache, &fPCKMap);
	if (fCCU == NULL || fPCK == NULL) {
		IOLog("PDPowerVR: cannot map the ccu or the pck\n");
		return false;
	}

	fDev = (struct pvr_device *)IOMallocZero(sizeof(*fDev));
	fQuiesceLock = IOLockAlloc();
	if (fDev == NULL || fQuiesceLock == NULL)
		return false;
	// the watchdog deadline and a restart both stop the gpu's dma before the board resets
	setProperty("pd-quiesce", true);
	fRestartNotifier = registerPrioritySleepWakeInterest(&PDPowerVR::restartHandler, this, NULL);
	if (gWaitLock == NULL)
		gWaitLock = IOLockAlloc();
	if (pvr_gem_init() != 0)
		return false;
	fNub = provider;

	// the firmware boot polls for seconds, keep it off the matching thread
	retain();
	if (kernel_thread_start(&PDPowerVR::bringUpThread, this, &th) != KERN_SUCCESS) {
		release();
		return false;
	}
	thread_deallocate(th);

	registerService();
	return true;
}

void
PDPowerVR::stop(IOService *provider)
{
	if (fRestartNotifier != NULL) {
		fRestartNotifier->remove();
		fRestartNotifier = NULL;
	}
	quiesce("stop");
	if (fIntSource != NULL) {
		fIntSource->disable();
		if (fWorkLoop != NULL)
			fWorkLoop->removeEventSource(fIntSource);
		OSSafeReleaseNULL(fIntSource);
	}
	super::stop(provider);
}

void
PDPowerVR::free(void)
{
	if (fDev != NULL) {
		pvr_fw_fini(fDev);
		IOFree(fDev, sizeof(*fDev));
		fDev = NULL;
	}
	OSSafeReleaseNULL(fRegMap);
	OSSafeReleaseNULL(fCCUMap);
	OSSafeReleaseNULL(fPCKMap);
	if (fQuiesceLock != NULL) {
		IOLockFree(fQuiesceLock);
		fQuiesceLock = NULL;
	}
	super::free();
}

void
PDPowerVR::bringUpThread(void *arg, wait_result_t)
{
	PDPowerVR *self = (PDPowerVR *)arg;

	self->bringUp();
	self->release();
	thread_terminate(current_thread());
}

void
PDPowerVR::logPowerState(const char *when)
{
	IOLog("PDPowerVR: %s: gpu_top ppu 0x%x/0x%x gpu_core ppu 0x%x/0x%x, pll-gpu 0x%08x gpu0 0x%08x "
	    "bgr 0x%08x ahb 0x%08x mbus 0x%08x\n", when,
	    pck(kPCK_DOMAIN(kPCK_GPU_TOP) + kPPU_PWPR), pck(kPCK_DOMAIN(kPCK_GPU_TOP) + kPPU_PWSR),
	    pck(kPCK_DOMAIN(kPCK_GPU_CORE) + kPPU_PWPR), pck(kPCK_DOMAIN(kPCK_GPU_CORE) + kPPU_PWSR),
	    ccu(kCCU_PLL_GPU), ccu(kCCU_GPU0_CLK), ccu(kCCU_GPU0_BGR), ccu(kCCU_AHB_GATE), ccu(kCCU_MBUS_GATE));
}

// what the pck-600 genpd does for a domain that is off: program the delays, ask for on, wait
bool
PDPowerVR::powerDomainOn(uint32_t domain)
{
	uint32_t base = kPCK_DOMAIN(domain);

	if ((pck(base + kPPU_PWSR) & kPPU_POLICY_MASK) == kPPU_ON)
		return true;
	pckWrite(base + kPPU_DEV_CTRL0_DELAY, 0x1f1f1f);
	pckWrite(base + kPPU_DEV_CTRL1_DELAY, 0x1f1f);
	pckWrite(base + kPPU_LOGIC_SW0_DELAY, 0x8080808);
	pckWrite(base + kPPU_LOGIC_SW1_DELAY, 0x808);
	pckWrite(base + kPPU_OFF2ON_DELAY, 0x8);
	pckWrite(base + kPPU_PWPR, (pck(base + kPPU_PWPR) & ~kPPU_POLICY_MASK) | kPPU_ON);
	for (int i = 0; i < 1000; i++) {
		if ((pck(base + kPPU_PWSR) & kPPU_POLICY_MASK) == kPPU_ON) {
			IOLog("PDPowerVR: pck domain %u powered on\n", domain);
			return true;
		}
		IODelay(10);
	}
	IOLog("PDPowerVR: pck domain %u stays off, status 0x%x\n", domain, pck(base + kPPU_PWSR));
	return false;
}

bool
PDPowerVR::enableGpuPll(void)
{
	uint32_t v = ccu(kCCU_PLL_GPU);

	if ((v & (kPLL_ENABLE | kPLL_LOCK | kPLL_OUTPUT)) != (kPLL_ENABLE | kPLL_LOCK | kPLL_OUTPUT)) {
		if ((v & ~kPLL_CTRL_BITS) != kPLL_GPU_FACTORS) {
			IOLog("PDPowerVR: pll-gpu 0x%08x has factors linux never set, leaving it\n", v);
			return false;
		}
		v |= kPLL_LDO_EN;
		ccuWrite(kCCU_PLL_GPU, v);
		IODelay(5);
		v |= kPLL_ENABLE | kPLL_LOCK_EN;
		ccuWrite(kCCU_PLL_GPU, v);
		for (int i = 0; i < 1000 && (ccu(kCCU_PLL_GPU) & kPLL_LOCK) == 0; i++) {
			IODelay(10);
		}
		if ((ccu(kCCU_PLL_GPU) & kPLL_LOCK) == 0) {
			IOLog("PDPowerVR: pll-gpu never locked, 0x%08x\n", ccu(kCCU_PLL_GPU));
			return false;
		}
		ccuWrite(kCCU_PLL_GPU, ccu(kCCU_PLL_GPU) | kPLL_OUTPUT);
	}

	v = ccu(kCCU_PLL_GPU);
	fCoreClockHz = (uint32_t)(24000000ULL * (((v >> 8) & 0xff) + 1) / (((v >> 20) & 0x7) + 1));
	return true;
}

// the reverse of powerUp() as linux's power_off sequence: reset, bus gate, gpu0, pll-gpu
void
PDPowerVR::powerDownGpu(void)
{
	ccuWrite(kCCU_GPU0_BGR, ccu(kCCU_GPU0_BGR) & ~kGPU0_BGR_RESET);
	IODelay(1);
	ccuWrite(kCCU_GPU0_BGR, ccu(kCCU_GPU0_BGR) & ~kGPU0_BGR_GATE);
	ccuWrite(kCCU_GPU0_CLK, kGPU0_CLK_UPD_KEY);
	ccuWrite(kCCU_PLL_GPU, ccu(kCCU_PLL_GPU) & ~(kPLL_ENABLE | kPLL_LOCK_EN | kPLL_OUTPUT));
}

// stop everything that can write memory: the firmware is idled and stopped through the kccb
// when it answers, then the core goes back into reset with its clocks off
void
PDPowerVR::quiesce(const char *why)
{
	struct pvr_device *dev = fDev;
	int err = 0;

	if (fQuiesceLock == NULL)
		return;
	IOLockLock(fQuiesceLock);
	if (fQuiesced) {
		IOLockUnlock(fQuiesceLock);
		return;
	}
	fQuiesced = true;
	fReady = false;
	if (fIntSource != NULL)
		fIntSource->disable();
	if (dev != NULL && dev->fw_dev.booted) {
		err = pvr_power_fw_disable(dev, dev->lost);
		dev->fw_dev.booted = false;
	}
	// a firmware that never idled still stops here: everything into soft reset, then the core's reset
	if (dev != NULL && dev->regs != NULL && (ccu(kCCU_GPU0_BGR) & kGPU0_BGR_RESET) != 0) {
		pvr_cr_write64(dev, ROGUE_CR_SOFT_RESET, ROGUE_CR_SOFT_RESET_MASKFULL);
		(void)pvr_cr_read64(dev, ROGUE_CR_SOFT_RESET);
	}
	powerDownGpu();
	IOLockUnlock(fQuiesceLock);
	IOLog("PDPowerVR: quiesced for %s (firmware stop %d), gpu0 0x%08x bgr 0x%08x pll-gpu 0x%08x\n", why, err,
	    ccu(kCCU_GPU0_CLK), ccu(kCCU_GPU0_BGR), ccu(kCCU_PLL_GPU));
}

IOReturn
PDPowerVR::restartHandler(void *target, void *, UInt32 messageType, IOService *, void *, vm_size_t)
{
	PDPowerVR *self = (PDPowerVR *)target;

	if (messageType == kIOMessageSystemWillRestart || messageType == kIOMessageSystemWillPowerOff)
		self->quiesce(messageType == kIOMessageSystemWillRestart ? "restart" : "power off");
	return kIOReturnSuccess;
}

// the watchdog deadline arrives as a restart message to every service marked pd-quiesce
IOReturn
PDPowerVR::message(UInt32 type, IOService *provider, void *argument)
{
	if (type == kIOMessageSystemWillRestart || type == kIOMessageSystemWillPowerOff) {
		quiesce("watchdog deadline");
		return kIOReturnSuccess;
	}
	return super::message(type, provider, argument);
}

bool
PDPowerVR::powerUp(void)
{
	logPowerState("before power up");

	// a core still out of reset is left from before a reset that did not reach it: stop it first
	if (ccu(kCCU_GPU0_BGR) & kGPU0_BGR_RESET) {
		IOLog("PDPowerVR: gpu found running (bgr 0x%08x gpu0 0x%08x), putting it back in reset\n",
		    ccu(kCCU_GPU0_BGR), ccu(kCCU_GPU0_CLK));
		powerDownGpu();
		IODelay(10);
	}

	if (!powerDomainOn(kPCK_GPU_TOP))
		return false;
	// the ahb and mbus master gates are critical clocks linux never touches, they must already be on
	if ((ccu(kCCU_AHB_GATE) & kAHB_GPU0) == 0 || (ccu(kCCU_MBUS_GATE) & kMBUS_GPU0) == 0) {
		IOLog("PDPowerVR: gpu ahb or mbus master gate is off, not touching the gpu\n");
		return false;
	}
	if (!enableGpuPll())
		return false;

	// gpu0 from pll-gpu undivided, every write to this register carries the update key
	ccuWrite(kCCU_GPU0_CLK, kGPU0_CLK_UPD_KEY | kGPU0_CLK_SRC_PLL_GPU | kGPU0_CLK_DIV1);
	ccuWrite(kCCU_GPU0_CLK, kGPU0_CLK_UPD_KEY | kGPU0_CLK_ENABLE | kGPU0_CLK_SRC_PLL_GPU | kGPU0_CLK_DIV1);
	ccuWrite(kCCU_GPU0_BGR, ccu(kCCU_GPU0_BGR) | kGPU0_BGR_GATE);
	// at least 32 gpu clocks between the clock and the reset
	IODelay(1);
	ccuWrite(kCCU_GPU0_BGR, ccu(kCCU_GPU0_BGR) | kGPU0_BGR_RESET);

	for (int i = 0; i < 1000; i++) {
		if ((pck(kPCK_DOMAIN(kPCK_GPU_CORE) + kPPU_PWSR) & kPPU_POLICY_MASK) == kPPU_ON)
			break;
		IODelay(10);
	}
	logPowerState("after power up");

	if ((ccu(kCCU_GPU0_CLK) & kGPU0_CLK_ENABLE) == 0 ||
	    (ccu(kCCU_GPU0_BGR) & (kGPU0_BGR_GATE | kGPU0_BGR_RESET)) != (kGPU0_BGR_GATE | kGPU0_BGR_RESET) ||
	    (pck(kPCK_DOMAIN(kPCK_GPU_TOP) + kPPU_PWSR) & kPPU_POLICY_MASK) != kPPU_ON) {
		IOLog("PDPowerVR: gpu clock, reset or power did not stick\n");
		return false;
	}
	IOLog("PDPowerVR: gpu powered, clocked at %u hz, out of reset\n", fCoreClockHz);
	return true;
}

void
PDPowerVR::bringUp(void)
{
	struct pvr_device *dev = fDev;
	uint32_t fwSize = 0;
	int err;

	if (!powerUp())
		return;

	dev->regs = fRegs;
	dev->regs_phys = fRegsPhys;
	dev->core_clock_hz = fCoreClockHz;

	IOLog("PDPowerVR: reading the core id at 0x%llx\n", fRegsPhys + ROGUE_CR_CORE_ID__PBVNC);
	pvr_load_gpu_id(dev);

	dev->fw_dev.image = pd_pvr_fw_blob(&fwSize);
	dev->fw_dev.image_size = fwSize;
	IOLog("PDPowerVR: firmware rogue_%u.%u.%u.%u_v1.fw, %u bytes, built in\n",
	    dev->gpu_id.b, dev->gpu_id.v, dev->gpu_id.n, dev->gpu_id.c, fwSize);
	err = pvr_fw_validate_init_device_info(dev);
	if (err) {
		IOLog("PDPowerVR: firmware does not fit this gpu (%d)\n", err);
		return;
	}
	IOLog("PDPowerVR: features: mips %d, phys bus %llu bits, clusters %llu, slc %llu kb line %llu bits, "
	    "xe_tpu2 %d, pbe2_in_xe %d, sys_bus_secure_reset %d, brn63553 %d brn71242 %d\n",
	    dev->features.has_mips, dev->features.phys_bus_width, dev->features.num_clusters,
	    dev->features.slc_size_in_kilobytes, dev->features.slc_cache_line_size_bits,
	    dev->features.has_xe_tpu2, dev->features.has_pbe2_in_xe,
	    dev->features.has_sys_bus_secure_reset, dev->quirks.has_brn63553, dev->quirks.has_brn71242);

	err = pvr_fw_init(dev);
	if (err) {
		IOLog("PDPowerVR: firmware did not boot (%d)\n", err);
		return;
	}
	setProperty("pvr-fw-booted", true);
	IOLog("PDPowerVR: firmware processor up, handshake done\n");

	if (!attachInterrupt())
		IOLog("PDPowerVR: no interrupt, waits fall back to polling\n");
	exerciseKCCB();
	if (!dev->fw_dev.booted || dev->lost)
		return;
	pvr_device_jobs_init(dev);
	exerciseVM();
	fReady = true;
	// pdpvrtest=0 skips the boot-time job
	{
		uint32_t test = 1;

		if (!PE_parse_boot_argn("pdpvrtest", &test, sizeof(test)) || test != 0)
			pvr_selftest_null_compute(dev);
	}
	IOLog("PDPowerVR: open for user clients\n");
}

// the gic spis the board's tree names, as the PDArmGIC controller's 32-bit intids
bool
PDPowerVR::attachInterrupt(void)
{
	OSData *prop = OSDynamicCast(OSData, fNub->getProperty("interrupts"));
	const OSSymbol *gic = OSSymbol::withCString("PDArmGIC");
	OSArray *controllers, *specifiers;
	const uint32_t *cells;
	uint32_t n;
	bool swapped = false;

	if (prop == NULL || prop->getLength() < 12 || gic == NULL) {
		IOLog("PDPowerVR: the gpu node names no interrupts\n");
		OSSafeReleaseNULL(gic);
		return false;
	}
	cells = (const uint32_t *)prop->getBytesNoCopy();
	n = prop->getLength() / 12;
	controllers = OSArray::withCapacity(n);
	specifiers = OSArray::withCapacity(n);
	for (uint32_t i = 0; i < n && controllers != NULL && specifiers != NULL; i++) {
		uint32_t type = cells[i * 3], num = cells[i * 3 + 1], intid;
		OSData *spec;

		// the tree may still hold its cells big-endian
		if (type > 1 || num > 987) {
			type = OSSwapInt32(type);
			num = OSSwapInt32(num);
			swapped = true;
		}
		intid = num + (type == 0 ? 32 : 16);
		spec = OSData::withBytes(&intid, sizeof(intid));
		controllers->setObject(gic);
		specifiers->setObject(spec);
		OSSafeReleaseNULL(spec);
		IOLog("PDPowerVR: interrupt %u is gic intid %u%s\n", i, intid, swapped ? " (swapped cells)" : "");
	}
	if (controllers == NULL || specifiers == NULL) {
		OSSafeReleaseNULL(controllers);
		OSSafeReleaseNULL(specifiers);
		gic->release();
		return false;
	}
	fNub->setProperty(gIOInterruptControllersKey, controllers);
	fNub->setProperty(gIOInterruptSpecifiersKey, specifiers);
	controllers->release();
	specifiers->release();
	gic->release();

	fWorkLoop = getWorkLoop();
	if (fWorkLoop == NULL)
		return false;
	// index 0 is IRQGPU, the dvfs line is left alone
	fIntSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
	    OSMemberFunctionCast(IOInterruptEventAction, this, &PDPowerVR::fwInterrupt),
	    OSMemberFunctionCast(IOFilterInterruptAction, this, &PDPowerVR::fwInterruptFilter),
	    fNub, 0);
	if (fIntSource == NULL || fWorkLoop->addEventSource(fIntSource) != kIOReturnSuccess) {
		IOLog("PDPowerVR: cannot register the gpu interrupt\n");
		OSSafeReleaseNULL(fIntSource);
		return false;
	}
	pvr_fw_irq_clear(fDev);
	fIntSource->enable();
	IOLog("PDPowerVR: gpu interrupt registered\n");
	return true;
}

// primary interrupt: the mips wrapper event is the only source, clear it so the line drops
bool
PDPowerVR::fwInterruptFilter(IOFilterInterruptEventSource *)
{
	if (fDev == NULL || !pvr_fw_irq_pending(fDev))
		return false;
	pvr_fw_irq_clear(fDev);
	fDev->irq_count++;
	return true;
}

void
PDPowerVR::fwInterrupt(IOInterruptEventSource *, int)
{
	if (fDev->fw_dev.booted) {
		pvr_fwccb_process(fDev);
		if (fReady)
			pvr_queue_process_all(fDev);
	}
	pd_pvr_wake(fDev);
}

// milestone 2 step 2: a health check, a cache flush, then a full firmware power cycle
void
PDPowerVR::exerciseKCCB(void)
{
	struct pvr_device *dev = fDev;
	uint32_t rtn = 0, irqs;
	uint64_t t0;
	int err;

	t0 = pd_now_us();
	irqs = dev->irq_count;
	err = pvr_kccb_health_check(dev, &rtn);
	IOLog("PDPowerVR: health check %s (%d), rtn 0x%x, %llu us, %u interrupts, kccb rd %u wr %u\n",
	    err ? "failed" : "acknowledged", err, rtn, pd_now_us() - t0, dev->irq_count - irqs,
	    dev->kccb.ccb.ctrl->read_offset, dev->kccb.ccb.ctrl->write_offset);
	if (err)
		return;

	t0 = pd_now_us();
	pvr_mmu_flush_request_all(dev);
	err = pvr_mmu_flush_exec(dev, true);
	IOLog("PDPowerVR: mmu cache flush %s (%d), %llu us\n", err ? "failed" : "done", err, pd_now_us() - t0);
	if (err)
		return;

	t0 = pd_now_us();
	err = pvr_power_fw_disable(dev, false);
	IOLog("PDPowerVR: firmware idle, power off and stop %s (%d), %llu us, soft reset 0x%llx\n",
	    err ? "failed" : "done", err, pd_now_us() - t0, pvr_cr_read64(dev, ROGUE_CR_SOFT_RESET));
	if (err)
		return;
	t0 = pd_now_us();
	err = pvr_power_fw_enable(dev);
	IOLog("PDPowerVR: firmware restart %s (%d), %llu us\n", err ? "failed" : "done", err, pd_now_us() - t0);
	if (err) {
		dev->fw_dev.booted = false;
		return;
	}
	irqs = dev->irq_count;
	err = pvr_kccb_health_check(dev, &rtn);
	IOLog("PDPowerVR: health check after restart %s (%d), rtn 0x%x, %u interrupts, %u total, %u fwccb\n",
	    err ? "failed" : "acknowledged", err, rtn, dev->irq_count - irqs, dev->irq_count, dev->fwccb_count);
}

// milestone 2 step 3: a user vm context, a buffer mapped into the general heap, the mmu flush
void
PDPowerVR::exerciseVM(void)
{
	struct pvr_device *dev = fDev;
	struct pvr_vm_context *vm;
	struct pvr_gem_object *bo, *found;
	struct drm_pvr_heap heaps[DRM_PVR_HEAP_COUNT];
	uint64_t addr, off = 0, sz = 0, t0 = pd_now_us();
	uint32_t *cpu, fwaddr = 0;
	int err;

	pvr_heaps_get(dev, heaps, DRM_PVR_HEAP_COUNT);
	addr = heaps[DRM_PVR_HEAP_GENERAL].base + 0x100000;
	vm = pvr_vm_create_context(dev, true);
	if (IS_ERR(vm)) {
		IOLog("PDPowerVR: vm context failed (%ld)\n", PTR_ERR(vm));
		return;
	}
	pvr_fw_object_get_fw_addr(pvr_vm_get_fw_mem_context(vm), &fwaddr);
	bo = pvr_gem_object_create(dev, 3 * PAGE_SIZE, DRM_PVR_BO_ALLOW_CPU_USERSPACE_ACCESS);
	if (IS_ERR(bo)) {
		IOLog("PDPowerVR: buffer object failed (%ld)\n", PTR_ERR(bo));
		pvr_vm_context_put(vm);
		return;
	}
	cpu = (uint32_t *)pvr_gem_object_vmap(bo);
	if (!IS_ERR(cpu))
		cpu[0] = 0x50445652;
	err = pvr_vm_map(vm, bo, 0, addr, 3 * PAGE_SIZE);
	if (err == 0) {
		pvr_mmu_flush_request_all(dev);
		err = pvr_mmu_flush_exec(dev, true);
	}
	found = err == 0 ? pvr_vm_find_gem_object(vm, addr + PAGE_SIZE, &off, &sz) : NULL;
	IOLog("PDPowerVR: vm context root 0x%llx fw mem ctx 0x%x, bo %u runs first 0x%llx, mapped at 0x%llx: "
	    "%s (%d), lookup %s size 0x%llx\n",
	    pvr_vm_get_page_table_root_addr(vm), fwaddr, bo->sgt.nents, bo->sgt.sgl[0].dma_address, addr,
	    err ? "failed" : "ok", err, found == bo ? "ok" : "wrong", sz);
	pvr_gem_object_put(found);
	if (err == 0) {
		err = pvr_vm_unmap(vm, addr, 3 * PAGE_SIZE);
		IOLog("PDPowerVR: vm unmap %s (%d), %llu us total, kccb rd %u\n", err ? "failed" : "ok", err,
		    pd_now_us() - t0, dev->kccb.ccb.ctrl->read_offset);
	}
	pvr_gem_object_put(bo);
	pvr_vm_context_put(vm);
}
