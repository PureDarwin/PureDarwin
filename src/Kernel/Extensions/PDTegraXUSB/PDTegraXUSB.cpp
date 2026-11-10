// SPDX-License-Identifier: MIT
// tegra x1 xusb host bring-up over FreeBSD's tegra_xhci.c and tegra210_xusbpadctl.c excerpts

#include "PDTegraXUSB.h"

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOInterrupts.h>
#include <libkern/OSByteOrder.h>
#include <pexpert/pexpert.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/vm.h>

extern "C" {
#include "tegra210_gpu.h"
#include "tegra210_xusb.h"
#include <arm/nvidia/tegra_pmc.h>
vnode_t vfs_rootvnode(void);
}

#define super IOService
OSDefineMetaClassAndStructors(PDTegraXUSB, IOService);
OSDefineMetaClassAndStructors(PDTegraXHCINub, IOService);

// block addresses from the address map (tegra x1 trm 2.x)
#define kPhysCAR		0x60006000ULL
#define kPhysGPIO		0x6000d000ULL
#define kPhysMC			0x70019000ULL
#define kPhysFuse		0x7000f800ULL

// car peripheral indexes, 32 per word in the order l h u v w x y (freebsd tegra210_clk_per.c)
#define IDX_U(n)		(2 * 32 + (n))
#define IDX_W(n)		(4 * 32 + (n))
#define IDX_Y(n)		(6 * 32 + (n))
#define CLK_XUSB_HOST		IDX_U(25)
#define CLK_XUSB_DEV		IDX_U(31)
#define CLK_XUSB_GATE		IDX_W(15)
#define CLK_XUSB_SS		IDX_W(28)
#define RST_XUSB_PADCTL		IDX_W(14)
#define CLK_USB2_TRK		IDX_Y(18)

// mc smmu xusb host asid word, bit 31 turns translation on (tegra x1 trm 18.11.1.84)
#define MC_SMMU_XUSB_HOST_ASID	0x288
#define MC_SMMU_XUSB_DEV_ASID	0x28c

// gpio ports of 8 pins, 4 ports per 0x100 bank, masked writes at cnf 0x80, oe 0x90, out 0xa0 (tegra x1 trm 9.13)
#define GPIO_MSK_CNF		0x80
#define GPIO_MSK_OE		0x90
#define GPIO_MSK_OUT		0xa0
#define GPIO_CNF		0x00
#define GPIO_OE			0x10
#define GPIO_OUT		0x20

// vbus switches in the l4t tree: rtl-5v0 on pin 0x39 for usb_vbus1, en-usb-vbus2 on pin 0xe5 for usb_vbus2
static const uint32_t kVbusPins[] = { 0x39, 0xe5 };

#define kFirmwarePath		"/Library/Firmware/nvidia/tegra210/xusb.bin"
#define kRootWaitS		120
// the same image carried in the device tree, for a root that sits behind this host
#define kFirmwareProp		"pd,xusb-firmware"

// usb2 pads the host takes by default: 1 to 3, pad 0 stays with the chipidea device controller
#define kDefaultPortMask	0xe

static void
xusb_log(const char *fmt, ...)
{
	va_list ap;
	char buf[256];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	IOLog("PDTegraXUSB: %s", buf);
}

static uint32_t
boot_arg(const char *name, uint32_t dflt)
{
	uint32_t v = dflt;

	PE_parse_boot_argn(name, &v, sizeof(v));
	return v;
}

// pmc words through the firmware's sip call: freebsd tegra210_pmc.c PMC_SMC, read 0xaa, write 0xbb
static uint32_t
pmc_read(uint32_t off)
{
	register uint64_t x0 __asm__("x0") = 0xc2fffe00ull;
	register uint64_t x1 __asm__("x1") = 0xaa;
	register uint64_t x2 __asm__("x2") = off;
	register uint64_t x3 __asm__("x3") = 0;

	__asm__ volatile ("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : : "memory");
	return x0 == 0 ? (uint32_t)x1 : 0xdeadbeef;
}

static void
pmc_write(uint32_t off, uint32_t val)
{
	register uint64_t x0 __asm__("x0") = 0xc2fffe00ull;
	register uint64_t x1 __asm__("x1") = 0xbb;
	register uint64_t x2 __asm__("x2") = off;
	register uint64_t x3 __asm__("x3") = val;

	__asm__ volatile ("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : : "memory");
}

static IOMemoryMap *
map_phys(uint64_t phys, uint64_t len, volatile uint32_t **out)
{
	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(phys, len, kIODirectionInOut);
	IOMemoryMap *map = md != NULL ? md->map(kIOMapAnywhere | kIOMapInhibitCache) : NULL;

	OSSafeReleaseNULL(md);
	*out = map != NULL ? (volatile uint32_t *)map->getVirtualAddress() : NULL;
	return map;
}

static void
cache_clean(const void *va, uint64_t len)
{
	uint64_t p = (uint64_t)va & ~63ULL;
	uint64_t end = (uint64_t)va + len;

	for (; p < end; p += 64) {
		__asm__ volatile ("dc civac, %0" :: "r"(p) : "memory");
	}
	__asm__ volatile ("dsb sy" ::: "memory");
}

static bool
root_mounted(void)
{
	vnode_t root = vfs_rootvnode();

	if (root == NULLVP) return false;
	vnode_put(root);
	return true;
}

// the whole file into a kernel buffer, NULL when it is missing or implausible
static void *
read_file(const char *path, size_t *lenp)
{
	vfs_context_t ctx = vfs_context_create(NULL);
	vnode_t vp = NULLVP;
	struct vnode_attr va;
	void *buf = NULL;
	size_t len = 0;
	int resid = 0;
	int error;

	error = vnode_open(path, FREAD, 0, 0, &vp, ctx);
	if (error != 0) {
		vfs_context_rele(ctx);
		xusb_log("cannot open %s (%d)\n", path, error);
		return NULL;
	}

	VATTR_INIT(&va);
	VATTR_WANTED(&va, va_data_size);
	error = vnode_getattr(vp, &va, ctx);
	if (error == 0 && (va.va_data_size < sizeof(struct tegra_xusb_fw_hdr) || va.va_data_size > 1024 * 1024))
		error = EFBIG;
	if (error == 0) {
		len = (size_t)va.va_data_size;
		buf = IOMalloc(len);
		if (buf == NULL) error = ENOMEM;
	}
	if (error == 0) {
		error = vn_rdwr(UIO_READ, vp, (caddr_t)buf, (int)len, 0, UIO_SYSSPACE, 0,
		    vfs_context_ucred(ctx), &resid, vfs_context_proc(ctx));
		if (error == 0 && resid != 0) error = EIO;
	}

	vnode_close(vp, FREAD, ctx);
	vfs_context_rele(ctx);
	if (error != 0) {
		if (buf != NULL) IOFree(buf, len);
		xusb_log("cannot read %s (%d)\n", path, error);
		return NULL;
	}
	*lenp = len;
	return buf;
}

// the device tree copy, the loader stores a binary property of whole words as native cells
static void *
dt_firmware(IOService *provider, size_t *lenp)
{
	OSData *d = OSDynamicCast(OSData, provider->getProperty(kFirmwareProp));
	const uint8_t *src;
	uint8_t *buf;
	size_t len;

	if (d == NULL) return NULL;
	len = d->getLength();
	if (len < sizeof(struct tegra_xusb_fw_hdr) || len > 1024 * 1024) return NULL;
	buf = (uint8_t *)IOMalloc(len);
	if (buf == NULL) return NULL;

	src = (const uint8_t *)d->getBytesNoCopy();
	if (len % 4 != 0) {
		memcpy(buf, src, len);
	} else {
		for (size_t i = 0; i < len; i += 4) {
			uint32_t w;
			memcpy(&w, src + i, 4);
			w = OSSwapHostToBigInt32(w);
			memcpy(buf + i, &w, 4);
		}
	}
	xusb_log("firmware from the device tree, 0x%lx bytes\n", len);
	*lenp = len;
	return buf;
}

bool
PDTegraXUSB::readReg(IOService *provider)
{
	OSData *reg = OSDynamicCast(OSData, provider->getProperty("reg"));
	const uint64_t *c;

	// the loader's tree holds reg as native 64-bit address and size pairs, one per window
	if (reg == NULL || reg->getLength() < 4 * 16) return false;
	c = (const uint64_t *)reg->getBytesNoCopy();
	fNumReg = 0;
	for (int i = 0; i < 4; i++) {
		fRegPhys[i] = c[i * 2];
		fRegSize[i] = c[i * 2 + 1];
		fNumReg++;
	}
	return true;
}

bool
PDTegraXUSB::mapBlocks(void)
{
	fCARMap = map_phys(kPhysCAR, 0x1000, &fCAR);
	fFPCIMap = map_phys(fRegPhys[1], fRegSize[1], &fFPCI);
	fIPFSMap = map_phys(fRegPhys[2], fRegSize[2], &fIPFS);
	fPadMap = map_phys(fRegPhys[3], fRegSize[3], &fPad);
	fGPIOMap = map_phys(kPhysGPIO, 0x1000, &fGPIO);
	fMCMap = map_phys(kPhysMC, 0x1000, &fMC);
	return fCAR != NULL && fFPCI != NULL && fIPFS != NULL && fPad != NULL && fGPIO != NULL && fMC != NULL;
}

// freebsd tegra_powergate_sequence_power_up(): reset, power, clock, clamps off, out of reset
static bool
partition_up(enum tegra_powergate_id id, uint32_t clk, const char *name)
{
	int rv;

	tegra210_hwreset_by_idx(clk, true);
	if (!tegra_powergate_is_powered(id)) {
		tegra210_pgate_set_gate(clk, false);
		rv = tegra_powergate_power_on(id);
		if (rv != 0) {
			xusb_log("%s partition did not power up (%d)\n", name, rv);
			return false;
		}
	}
	tegra210_pgate_set_gate(clk, true);
	IODelay(20);
	rv = tegra_powergate_remove_clamping(id);
	if (rv != 0) {
		xusb_log("%s partition keeps its clamps (%d)\n", name, rv);
		return false;
	}
	tegra210_hwreset_by_idx(clk, false);
	return true;
}

bool
PDTegraXUSB::powerPartitions(void)
{
	// the xusb gate clock comes before xusba can power up (freebsd tegra_xhci.c)
	tegra210_pgate_set_gate(CLK_XUSB_GATE, true);
	if (!partition_up(TEGRA_POWERGATE_XUSBC, CLK_XUSB_HOST, "xusbc")) return false;
	if (!partition_up(TEGRA_POWERGATE_XUSBA, CLK_XUSB_SS, "xusba")) return false;
	if (boot_arg("pdxusbb", 1) && !partition_up(TEGRA_POWERGATE_XUSBB, CLK_XUSB_DEV, "xusbb")) return false;

	xusb_log("partitions up, pmc pwrgate 0x%08x clamp 0x%08x\n", pmc_read(0x38), pmc_read(0x2c));
	return true;
}

void
PDTegraXUSB::setupPads(void)
{
	uint32_t mask = boot_arg("pdxusbports", kDefaultPortMask) & 0xf;
	bool on = false;

	tegra210_xusb_dump_padctl("before");
	if (tegra210_hwreset_is_asserted(RST_XUSB_PADCTL) == 1) {
		xusb_log("padctl was in reset\n");
		tegra210_hwreset_by_idx(RST_XUSB_PADCTL, false);
		IODelay(10);
	}

	// the fuse block reads back only while its clock runs (freebsd tegra_efuse.c)
	tegra210_pgate_get_gate(TEGRA210_CLK_FUSE, &on);
	if (!on) tegra210_pgate_set_gate(TEGRA210_CLK_FUSE, true);
	fFuseMap = map_phys(kPhysFuse, 0x400, &fFuse);
	tegra210_xusb_padctl_calibration();

	tegra210_pgate_set_gate(CLK_USB2_TRK, true);
	tegra210_xusb_padctl_common();
	for (int p = 1; p < 4; p++) {
		if (mask & (1u << p)) tegra210_xusb_padctl_usb2_port(p);
	}
	tegra210_xusb_dump_padctl("after");
}

// both vbus switches as gpio outputs driven high
void
PDTegraXUSB::vbusOn(void)
{
	if (!boot_arg("pdxusbvbus", 1)) return;

	for (unsigned i = 0; i < sizeof(kVbusPins) / sizeof(kVbusPins[0]); i++) {
		uint32_t port = kVbusPins[i] / 8, bit = kVbusPins[i] % 8;
		uint32_t off = (port / 4) * 0x100 + (port % 4) * 4;
		uint32_t m = 1u << (bit + 8), v = 1u << bit;

		xusb_log("vbus pin 0x%x before: cnf 0x%04x oe 0x%04x out 0x%04x\n", kVbusPins[i],
		    fGPIO[(off + GPIO_CNF) / 4], fGPIO[(off + GPIO_OE) / 4], fGPIO[(off + GPIO_OUT) / 4]);
		fGPIO[(off + GPIO_MSK_OUT) / 4] = m | v;
		fGPIO[(off + GPIO_MSK_OE) / 4] = m | v;
		fGPIO[(off + GPIO_MSK_CNF) / 4] = m | v;
	}
}

void
PDTegraXUSB::smmuCheck(void)
{
	uint32_t host = fMC[MC_SMMU_XUSB_HOST_ASID / 4];

	xusb_log("mc smmu xusb host asid 0x%08x dev asid 0x%08x\n", host, fMC[MC_SMMU_XUSB_DEV_ASID / 4]);
	if ((host & (1u << 31)) && boot_arg("pdxusbsmmu", 0) == 0) {
		fMC[MC_SMMU_XUSB_HOST_ASID / 4] = host & ~(1u << 31);
		xusb_log("smmu translation for the host turned off, now 0x%08x\n", fMC[MC_SMMU_XUSB_HOST_ASID / 4]);
	}
}

// the image goes whole into contiguous memory below 4 GB, the falcon pages its code from there
bool
PDTegraXUSB::loadFirmware(void)
{
	const struct tegra_xusb_fw_hdr *hdr;
	size_t len = 0;
	uint32_t imgLen;
	void *file;
	int rv;

	if (tegra210_xusb_fw_loaded()) return true;

	file = dt_firmware(fProvider, &len);
	if (file == NULL) {
		for (int i = 0; i < kRootWaitS * 10 && !root_mounted(); i++) {
			IOSleep(100);
		}
		if (!root_mounted()) {
			xusb_log("no root file system, no firmware\n");
			return false;
		}
		file = read_file(kFirmwarePath, &len);
	}
	if (file == NULL) return false;

	hdr = (const struct tegra_xusb_fw_hdr *)file;
	if (hdr->fwimg_len == 0 || hdr->fwimg_len > len) {
		xusb_log("firmware length 0x%x does not fit the 0x%lx byte file\n", hdr->fwimg_len, len);
		IOFree(file, len);
		return false;
	}
	// hdr points into file, which is freed before the log below
	imgLen = hdr->fwimg_len;
	fFirmware = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
	    kIODirectionInOut | kIOMemoryPhysicallyContiguous, round_page(len), 0x00000000fffff000ULL);
	// load_fw writes ILOAD_BASE_HI too, so dram above 4 GB works when the low range is taken
	if (fFirmware == NULL) {
		fFirmware = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
		    kIODirectionInOut | kIOMemoryPhysicallyContiguous, round_page(len), 0x00000003fffff000ULL);
	}
	if (fFirmware == NULL || fFirmware->prepare() != kIOReturnSuccess) {
		xusb_log("no contiguous memory for the firmware\n");
		IOFree(file, len);
		return false;
	}
	memcpy(fFirmware->getBytesNoCopy(), file, len);
	cache_clean(fFirmware->getBytesNoCopy(), len);
	IOFree(file, len);

	uint64_t pa = fFirmware->getPhysicalSegment(0, NULL, 0);
	xusb_log("firmware 0x%lx bytes, image 0x%x, at pa 0x%llx\n", len, imgLen, pa);
	rv = tegra210_xusb_load_fw(fFirmware->getBytesNoCopy(), pa);
	if (rv != 0) {
		xusb_log("falcon did not start (%d), cpuctl 0x%08x\n", rv, tegra210_xusb_falcon_cpuctl());
		return false;
	}
	return true;
}

// the xhci window and the host interrupt (the first of the node's), the gic specifier turned into an intid
bool
PDTegraXUSB::publishNub(void)
{
	OSArray *ctrls = OSDynamicCast(OSArray, fProvider->getProperty(gIOInterruptControllersKey));
	OSArray *specs = OSDynamicCast(OSArray, fProvider->getProperty(gIOInterruptSpecifiersKey));
	OSData *spec = specs != NULL ? OSDynamicCast(OSData, specs->getObject(0)) : NULL;
	IODeviceMemory *mem;
	OSArray *memArray, *c, *s;
	OSData *intid;
	uint32_t vec;

	if (ctrls == NULL || spec == NULL) {
		xusb_log("the node has no interrupt\n");
		return false;
	}
	// spis start at intid 32 (arm gic architecture), a three-cell specifier is type, number, flags
	if (spec->getLength() == 12) {
		const uint32_t *w = (const uint32_t *)spec->getBytesNoCopy();

		vec = w[1] + (w[0] == 0 ? 32 : 16);
	} else {
		vec = *(const uint32_t *)spec->getBytesNoCopy();
	}

	fNub = OSTypeAlloc(PDTegraXHCINub);
	if (fNub == NULL || !fNub->init()) return false;
	fNub->setName("pd,tegra210-xhci");
	mem = IODeviceMemory::withRange(fRegPhys[0], fRegSize[0]);
	memArray = OSArray::withObjects((const OSObject **)&mem, 1);
	fNub->setDeviceMemory(memArray);
	OSSafeReleaseNULL(mem);
	OSSafeReleaseNULL(memArray);

	intid = OSData::withBytes(&vec, sizeof(vec));
	s = OSArray::withObjects((const OSObject **)&intid, 1);
	OSObject *ctrl = ctrls->getObject(0);
	c = OSArray::withObjects((const OSObject **)&ctrl, 1);
	fNub->setProperty(gIOInterruptControllersKey, c);
	fNub->setProperty(gIOInterruptSpecifiersKey, s);
	OSSafeReleaseNULL(intid);
	OSSafeReleaseNULL(s);
	OSSafeReleaseNULL(c);

	if (!fNub->attach(this)) return false;
	fNub->registerService();
	xusb_log("xhci nub at 0x%llx size 0x%llx, interrupt %u\n", fRegPhys[0], fRegSize[0], vec);
	return true;
}

void
PDTegraXUSB::mailboxThread(void *arg, wait_result_t wr)
{
	PDTegraXUSB *self = (PDTegraXUSB *)arg;
	int n = 0;

	while (self->fMailboxRunning) {
		int type = tegra210_xusb_mbox_poll();

		if (type >= 0 && n++ < 32) xusb_log("falcon message %d\n", type);
		IOSleep(10);
	}
	thread_terminate(current_thread());
}

void
PDTegraXUSB::bringup(void)
{
	struct tegra210_gpu_bus gbus;
	struct tegra210_xusb_bus xbus;
	thread_t t = THREAD_NULL;
	int rv;

	if (!mapBlocks()) {
		xusb_log("cannot map the xusb blocks\n");
		return;
	}
	bzero(&gbus, sizeof(gbus));
	gbus.car = fCAR;
	gbus.pmc_read = pmc_read;
	gbus.pmc_write = pmc_write;
	tegra210_gpu_attach(&gbus);

	bzero(&xbus, sizeof(xbus));
	xbus.car = fCAR;
	xbus.padctl = fPad;
	xbus.fpci = fFPCI;
	xbus.ipfs = fIPFS;
	xbus.log = xusb_log;
	tegra210_xusb_attach(&xbus);

	xusb_log("osc %llu Hz, pmc pwrgate 0x%08x clamp 0x%08x\n", tegra210_osc_freq(), pmc_read(0x38), pmc_read(0x2c));
	tegra210_xusb_dump_car("before");
	if (tegra210_xusb_setup_clocks() != 0) return;
	if (!powerPartitions()) return;
	tegra210_xusb_dump_car("after");

	// the fuse map arrives in setupPads, so the bus is attached again with it
	setupPads();
	xbus.fuse = fFuse;
	tegra210_xusb_attach(&xbus);
	vbusOn();
	smmuCheck();

	tegra210_xusb_init_ipfs(fRegPhys[0]);
	if (!loadFirmware()) return;

	rv = tegra210_xusb_mbox_send_cmd(1, 0);
	xusb_log("msg_enabled %s (%d), cpuctl 0x%08x\n", rv == 0 ? "acked" : "failed", rv, tegra210_xusb_falcon_cpuctl());
	if (rv != 0) return;

	fMailboxRunning = true;
	if (kernel_thread_start(&PDTegraXUSB::mailboxThread, this, &t) == KERN_SUCCESS) thread_deallocate(t);

	if (!publishNub()) xusb_log("no nub for the xhci\n");
}

void
PDTegraXUSB::bringupThread(void *arg, wait_result_t wr)
{
	PDTegraXUSB *self = (PDTegraXUSB *)arg;

	self->bringup();
	self->release();
	thread_terminate(current_thread());
}

bool
PDTegraXUSB::start(IOService *provider)
{
	thread_t t = THREAD_NULL;

	if (!super::start(provider)) return false;
	if (boot_arg("pdtegraxusb", 1) == 0) {
		IOLog("PDTegraXUSB: off (pdtegraxusb=0)\n");
		return false;
	}
	if (!readReg(provider)) {
		IOLog("PDTegraXUSB: %s has no four reg windows\n", provider->getName());
		return false;
	}
	fProvider = provider;
	IOLog("PDTegraXUSB: xhci 0x%llx fpci 0x%llx ipfs 0x%llx padctl 0x%llx\n", fRegPhys[0], fRegPhys[1],
	    fRegPhys[2], fRegPhys[3]);

	// firmware comes off the root file system, so the work runs apart from matching
	retain();
	if (kernel_thread_start(&PDTegraXUSB::bringupThread, this, &t) != KERN_SUCCESS) {
		release();
		return false;
	}
	thread_deallocate(t);
	registerService();
	return true;
}

void
PDTegraXUSB::free(void)
{
	OSSafeReleaseNULL(fNub);
	OSSafeReleaseNULL(fFirmware);
	OSSafeReleaseNULL(fCARMap);
	OSSafeReleaseNULL(fPadMap);
	OSSafeReleaseNULL(fFPCIMap);
	OSSafeReleaseNULL(fIPFSMap);
	OSSafeReleaseNULL(fFuseMap);
	OSSafeReleaseNULL(fGPIOMap);
	OSSafeReleaseNULL(fMCMap);
	super::free();
}
