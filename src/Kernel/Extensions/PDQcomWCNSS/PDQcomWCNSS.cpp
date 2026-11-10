// PDQcomWCNSS.cpp - msm8916 wcnss: load the pronto firmware and start it through the pas calls
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOInterrupts.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOLib.h>
#include <pexpert/pexpert.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/fcntl.h>
#include <libkern/OSByteOrder.h>
#include <kern/clock.h>

#include "PDQcomWCNSS.h"
#include "PDQcomSMEM.h"
#include "PDQcomSMD.h"

// the uz801 device tree's carve-out, next to the tz block so xnu keeps one long run of ram
// source: openstick xnu-port/wifi-observations/pronto-scm-log5 (the 6 mb region linux used)
enum {
	kFwBase = 0x86800000,
	kFwSize = 0x600000,
	kPmuBase = 0x0a21b000,
	kPmuSize = 0x3000,
	kPmuProgress = 0x1084,
	kSmemBase = 0x86300000,
	kSmemSize = 0x100000,
	kApcsBase = 0x0b011000,
	kApcsIpc = 0x8,
	kProntoIpcBit = 17,
	kProntoEdge = 6,
	kProntoHost = 4,
	kRpmMsgRam = 0x60000,
	kRpmMsgRamSize = 0x8000,
	kRpmEdge = 15,
	kRpmIpcBit = 0,
};

// rpm requests: "req" magic, length, id, set, resource type and id, kvp length, kvps (lk2nd rpm-ipc.h,
// regulator.h), values from the rpm message ram fifo while wcnss started on this chain (openstick rpmfilm)
enum {
	kRpmReqMagic = 0x00716572,
	kRpmSmpa = 0x61706d73,
	kRpmLdoa = 0x616f646c,
	kRpmClka = 0x616b6c63,
	kRpmKeySwen = 0x6e657773,
	kRpmKeyCorner = 0x6e726f63,
	kRpmKeyMicroVolt = 0x00007675,
	kRpmKeyMilliAmp = 0x0000616d,
};

struct RpmVote {
	uint8_t set;
	uint32_t type, id;
	uint32_t key[3], value[3];
	uint8_t kvps;
};

// on before auth_and_reset, off once pronto runs (the rpm fifo in openstick rpmfilm): s3 at 1.3 v, cx (s1)
// and mx (l3) at corner 6 in both sets, the iris pa supply (l9) at 3.3 v and rf_clk2 (clock buffer 5)
static const RpmVote kVotesOn[] = {
	// the iris supplies linux had already enabled before wcnss started, l7 vddxo and l5 vdddig at 1.8 v
	// and s3 vddrfa, as its regulator summary shows them after a working scan (openstick rpmfilm)
	{ 0, kRpmLdoa, 7, { kRpmKeySwen, kRpmKeyMicroVolt, kRpmKeyMilliAmp }, { 1, 1800000, 10 }, 3 },
	{ 0, kRpmLdoa, 5, { kRpmKeySwen, kRpmKeyMicroVolt, kRpmKeyMilliAmp }, { 1, 1800000, 10 }, 3 },
	{ 0, kRpmSmpa, 3, { kRpmKeySwen, kRpmKeyMilliAmp }, { 1, 100 }, 2 },
	{ 0, kRpmSmpa, 3, { kRpmKeyMicroVolt }, { 1300000 }, 1 },
	{ 0, kRpmSmpa, 1, { kRpmKeySwen }, { 1 }, 1 },
	{ 0, kRpmSmpa, 1, { kRpmKeyCorner }, { 6 }, 1 },
	{ 1, kRpmSmpa, 1, { kRpmKeyCorner }, { 6 }, 1 },
	{ 0, kRpmLdoa, 3, { kRpmKeySwen }, { 1 }, 1 },
	{ 0, kRpmLdoa, 3, { kRpmKeyCorner }, { 6 }, 1 },
	{ 1, kRpmLdoa, 3, { kRpmKeyCorner }, { 6 }, 1 },
	// the loads linux's regulator summary holds for the iris supplies: a pa supply without its
	// 515 ma vote stays in low power mode, the "ma" key from lk2nd regulator.h (KEY_CURRENT)
	{ 0, kRpmLdoa, 9, { kRpmKeySwen, kRpmKeyMicroVolt, kRpmKeyMilliAmp }, { 1, 3300000, 515 }, 3 },
	{ 0, kRpmClka, 5, { kRpmKeySwen }, { 1 }, 1 },
	{ 1, kRpmClka, 5, { kRpmKeySwen }, { 1 }, 1 },
};

static const RpmVote kVotesOff[] = {
	{ 0, kRpmClka, 5, { kRpmKeySwen }, { 0 }, 1 },
	{ 1, kRpmClka, 5, { kRpmKeySwen }, { 0 }, 1 },
	{ 0, kRpmLdoa, 9, { kRpmKeySwen }, { 0 }, 1 },
	{ 0, kRpmLdoa, 3, { kRpmKeyCorner }, { 0 }, 1 },
	{ 1, kRpmLdoa, 3, { kRpmKeyCorner }, { 0 }, 1 },
	{ 0, kRpmSmpa, 1, { kRpmKeyCorner }, { 0 }, 1 },
	{ 1, kRpmSmpa, 1, { kRpmKeyCorner }, { 0 }, 1 },
	{ 0, kRpmLdoa, 3, { kRpmKeySwen }, { 0 }, 1 },
	{ 0, kRpmSmpa, 1, { kRpmKeySwen }, { 0 }, 1 },
};

// wcnss_ctrl messages: u32 type, u32 length of the whole message, then the body
// source: openstick smem-before.bin of pd-prontoboot-pmu-transition, the WCNSS_CTRL fifo halves
enum {
	kCtrlVersionReq = 0x01000000,
	kCtrlVersionResp = 0x01000001,
	kCtrlNvReq = 0x01000002,
	kCtrlNvResp = 0x01000003,
	kNvFragment = 3072,
};

// sip pas calls in the arm smc calling convention, owner 2, as the tf-a service decodes them
// source: openstick tracerd/src/pronto_scm_log.c and xnu-port/wifi-observations/pronto-scm-log2..6
enum {
	kScmSvcAvailable = 0x42000601,
	kScmPasSupported = 0x42000207,
	kScmPasInitImage = 0x42000201,
	kScmPasMemSetup = 0x42000202,
	kScmPasAuthReset = 0x42000205,
	kPasWcnss = 6,
	kSmcUnknown = -1,
};

// elf32 program header fields the loader reads, from the elf specification
enum {
	kPtLoad = 1,
	kEhdrSize = 52,
	kPhdrSize = 32,
};

#define FW_DIR "/Library/Firmware/qcom/"

OSDefineMetaClassAndStructors(PDQcomWCNSS, IOService);

static void
smc(uint64_t *r)
{
	register uint64_t x0 __asm__("x0") = r[0];
	register uint64_t x1 __asm__("x1") = r[1];
	register uint64_t x2 __asm__("x2") = r[2];
	register uint64_t x3 __asm__("x3") = r[3];
	register uint64_t x4 __asm__("x4") = r[4];

	__asm__ volatile ("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4) : : "x5", "x6", "x7",
	    "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "memory");
	r[0] = x0;
	r[1] = x1;
}

// x0 is the status, x1 the result, as qti_syscall.c returns them
static int64_t
scm(uint32_t fid, uint64_t info, uint64_t a, uint64_t b, uint64_t c, uint64_t *result)
{
	uint64_t r[5] = { fid, info, a, b, c };

	smc(r);
	if (result != NULL)
		*result = r[1];
	return (int64_t)r[0];
}

// first device tree node whose nul-separated compatible list names want, retained
static IORegistryEntry *
findCompatible(const char *want)
{
	IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane), *found = NULL;
	size_t wl = strlen(want);

	if (root == NULL)
		return NULL;
	OSIterator *it = IORegistryIterator::iterateOver(root, gIODTPlane, kIORegistryIterateRecursively);
	IORegistryEntry *e;
	while (it != NULL && found == NULL && (e = OSDynamicCast(IORegistryEntry, it->getNextObject())) != NULL) {
		OSData *compat = OSDynamicCast(OSData, e->getProperty("compatible"));
		const char *c = compat != NULL ? (const char *)compat->getBytesNoCopy() : NULL;
		for (size_t off = 0; c != NULL && off < compat->getLength(); off += strlen(c + off) + 1) {
			if (strlen(c + off) == wl && strncmp(c + off, want, wl) == 0) {
				found = e;
				found->retain();
				break;
			}
		}
	}
	OSSafeReleaseNULL(it);
	root->release();
	return found;
}

static IOMemoryMap *
mapPhys(uint64_t pa, uint64_t len)
{
	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(pa, len, kIODirectionInOut);
	IOMemoryMap *map = md != NULL ? md->createMappingInTask(kernel_task, 0, kIOMapAnywhere | kIOMapInhibitCache) : NULL;

	OSSafeReleaseNULL(md);
	return map;
}

bool
PDQcomWCNSS::readFile(const char *name, uint8_t **data, uint32_t *size, uint32_t limit)
{
	char path[128];
	vfs_context_t ctx = vfs_context_create(NULL);
	vnode_t vp = NULLVP;
	int err = ENOENT;

	snprintf(path, sizeof(path), FW_DIR "%s", name);

	// the root filesystem may not be mounted yet when this runs from start
	for (int tries = 0; tries < 600; tries++) {
		err = vnode_open(path, FREAD, 0, 0, &vp, ctx);
		if (err == 0)
			break;
		IOSleep(1000);
	}
	if (err != 0) {
		IOLog("PDQcomWCNSS: %s: error %d\n", path, err);
		vfs_context_rele(ctx);
		return false;
	}

	struct vnode_attr va;
	VATTR_INIT(&va);
	VATTR_WANTED(&va, va_data_size);
	uint64_t len = 0;
	if (vnode_getattr(vp, &va, ctx) == 0)
		len = va.va_data_size;
	if (len == 0 || len > limit) {
		IOLog("PDQcomWCNSS: %s: unexpected size %llu\n", path, len);
		vnode_close(vp, FREAD, ctx);
		vfs_context_rele(ctx);
		return false;
	}

	uint8_t *buf = (uint8_t *)IOMallocData((vm_size_t)len);
	uio_t uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
	uio_addiov(uio, CAST_USER_ADDR_T(buf), (user_size_t)len);
	err = VNOP_READ(vp, uio, 0, ctx);
	if (err == 0 && uio_resid(uio) != 0)
		err = EIO;
	uio_free(uio);
	vnode_close(vp, FREAD, ctx);
	vfs_context_rele(ctx);
	if (err != 0) {
		IOFreeData(buf, (vm_size_t)len);
		IOLog("PDQcomWCNSS: %s: read error %d\n", path, err);
		return false;
	}
	*data = buf;
	*size = (uint32_t)len;
	return true;
}

// 32-bit stores only: a wider copy into the carve-out faulted under linux
// source: openstick xnu-port/wifi-observations/prontomem-openstick
static void
copyWords(volatile uint32_t *dst, const uint8_t *src, uint32_t len)
{
	for (uint32_t i = 0; i < len; i += 4) {
		uint32_t w;
		memcpy(&w, src + i, 4);
		dst[i / 4] = w;
	}
}

bool
PDQcomWCNSS::loadFirmware(void)
{
	uint8_t *mdt = NULL;
	uint32_t mdtSize = 0;
	bool ok = false;

	if (!readFile("wcnss.mdt", &mdt, &mdtSize, 64 * 1024))
		return false;

	uint16_t phnum = OSReadLittleInt16(mdt, 44);
	uint32_t phoff = OSReadLittleInt32(mdt, 28);
	if (mdtSize < kEhdrSize || memcmp(mdt, "\177ELF\001\001", 6) != 0 || OSReadLittleInt16(mdt, 42) != kPhdrSize ||
	    phnum == 0 || (uint64_t)phoff + phnum * kPhdrSize > mdtSize) {
		IOLog("PDQcomWCNSS: wcnss.mdt is not a 32-bit little-endian elf\n");
		goto out;
	}

	// the image is linked at 0x8b600000 and relocated as a whole to the carve-out base
	// source: openstick tracerd/src/prontoboot.c and pronto-boot-sync.bin (boot address 0x8e20)
	{
		uint32_t lowest = 0xffffffff, highest = 0;
		for (uint16_t i = 0; i < phnum; i++) {
			const uint8_t *ph = mdt + phoff + i * kPhdrSize;
			uint32_t paddr = OSReadLittleInt32(ph, 12), memsz = OSReadLittleInt32(ph, 20);
			if (OSReadLittleInt32(ph, 0) != kPtLoad || memsz == 0)
				continue;
			if (paddr < lowest)
				lowest = paddr;
			if (paddr + memsz > highest)
				highest = paddr + memsz;
		}
		if (lowest == 0xffffffff || highest - lowest > kFwSize) {
			IOLog("PDQcomWCNSS: wcnss.mdt segments do not fit the carve-out\n");
			goto out;
		}

		// the metadata goes to the monitor in a buffer below 4 gb
		IOBufferMemoryDescriptor *meta = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
		    kIODirectionInOut | kIOMemoryPhysicallyContiguous, mdtSize, 0xfffff000ULL);
		if (meta == NULL)
			goto out;
		memcpy(meta->getBytesNoCopy(), mdt, mdtSize);
		uint64_t metaPa = meta->getPhysicalSegment(0, NULL, 0);

		uint64_t res = 0;
		int64_t st = scm(kScmSvcAvailable, 1, kScmPasSupported & 0x3fffffff, 0, 0, &res);
		IOLog("PDQcomWCNSS: pas query %lld/%llu\n", st, res);
		st = scm(kScmPasSupported, 1, kPasWcnss, 0, 0, &res);
		if (st != 0 || res == 0) {
			IOLog("PDQcomWCNSS: pas unavailable for wcnss (%lld/%llu)\n", st, res);
			meta->release();
			goto out;
		}

		// arginfo 0x82: two arguments, the second a read-write buffer
		st = scm(kScmPasInitImage, 0x82, kPasWcnss, metaPa, 0, &res);
		meta->release();
		if (st != 0 || res != 0) {
			IOLog("PDQcomWCNSS: pas init_image failed (%lld/%llu)\n", st, res);
			goto out;
		}
		// the span rounded up to a page, 0x57fb00 here went out as 0x580000 under linux
		// source: openstick xnu-port/wifi-observations/pronto-scm-log5/dmesg.txt
		st = scm(kScmPasMemSetup, 3, kPasWcnss, kFwBase, (highest - lowest + 0xfff) & ~0xfffu, &res);
		if (st != 0 || res != 0) {
			IOLog("PDQcomWCNSS: pas mem_setup failed (%lld/%llu)\n", st, res);
			goto out;
		}

		IOMemoryMap *fwMap = mapPhys(kFwBase, kFwSize);
		if (fwMap == NULL)
			goto out;
		volatile uint32_t *fw = (volatile uint32_t *)fwMap->getVirtualAddress();
		bool copied = true;
		for (uint16_t i = 0; i < phnum && copied; i++) {
			const uint8_t *ph = mdt + phoff + i * kPhdrSize;
			uint32_t paddr = OSReadLittleInt32(ph, 12), filesz = OSReadLittleInt32(ph, 16);
			uint32_t memsz = OSReadLittleInt32(ph, 20), off = paddr - lowest;
			if (OSReadLittleInt32(ph, 0) != kPtLoad || memsz == 0)
				continue;
			if (((off | filesz | memsz) & 3) != 0 || filesz > memsz) {
				copied = false;
				break;
			}
			for (uint32_t j = filesz; j < memsz; j += 4)
				fw[(off + j) / 4] = 0;
			if (filesz == 0)
				continue;

			char name[16];
			uint8_t *seg = NULL;
			uint32_t segSize = 0;
			snprintf(name, sizeof(name), "wcnss.b%02u", i);
			copied = readFile(name, &seg, &segSize, kFwSize) && segSize == filesz;
			if (copied)
				copyWords(fw + off / 4, seg, filesz);
			if (seg != NULL)
				IOFreeData(seg, segSize);
		}
		__asm__ volatile ("dsb sy" ::: "memory");
		IOLog("PDQcomWCNSS: firmware %s, first word 0x%08x\n", copied ? "copied" : "copy failed", fw[0]);
		fwMap->release();
		if (!copied)
			goto out;

		st = scm(kScmPasAuthReset, 1, kPasWcnss, 0, 0, &res);
		if (st != 0 || res != 0) {
			IOLog("PDQcomWCNSS: pas auth_and_reset failed (%lld/%llu)\n", st, res);
			goto out;
		}
	}
	ok = true;

out:
	IOFreeData(mdt, mdtSize);
	return ok;
}

// the low byte of the pmu progress word reaches 0x2a once pronto has booted
// source: openstick xnu-port/wifi-observations/prontoboot-pmu-power and pronto-scm-final
bool
PDQcomWCNSS::waitProgress(uint8_t want, uint32_t ms)
{
	for (uint32_t t = 0; t <= ms; t += 10) {
		uint32_t v = fPmu[kPmuProgress / 4];
		if ((v & 0xff) == want) {
			IOLog("PDQcomWCNSS: progress 0x%08x after %u ms\n", v, t);
			return true;
		}
		IOSleep(10);
	}
	IOLog("PDQcomWCNSS: progress stuck at 0x%08x\n", fPmu[kPmuProgress / 4]);
	return false;
}

void
PDQcomWCNSS::ctrlRecv(void *ctx, const uint8_t *data, uint32_t len)
{
	PDQcomWCNSS *self = (PDQcomWCNSS *)ctx;
	uint32_t type = len >= 8 ? OSReadLittleInt32(data, 0) : 0;

	if (type == kCtrlVersionResp && len >= 12) {
		IOLog("PDQcomWCNSS: WCNSS_CTRL version %u.%u %u.%u\n", data[8], data[9], data[10], data[11]);
		self->fCtrlState = 1;
	} else if (type == kCtrlNvResp && len >= 9) {
		IOLog("PDQcomWCNSS: WCNSS_CTRL nv reply status %u\n", data[8]);
		self->fCtrlState = 3;
	} else {
		IOLog("PDQcomWCNSS: WCNSS_CTRL message 0x%08x, %u bytes\n", type, len);
	}
}

// the nv image goes out in 3072-byte fragments, the last one flagged
// source: the same fifo, fragments 8 and 9 of WCNSS_qcom_wlan_nv.bin with their 16-byte headers
bool
PDQcomWCNSS::sendNv(void)
{
	uint8_t *nv = NULL;
	uint32_t nvSize = 0;
	uint8_t *msg = (uint8_t *)IOMallocData(16 + kNvFragment);
	bool ok = readFile("WCNSS_qcom_wlan_nv.bin", &nv, &nvSize, 256 * 1024);

	for (uint32_t off = 0, frag = 0; ok && off < nvSize; off += kNvFragment, frag++) {
		uint32_t n = nvSize - off < kNvFragment ? nvSize - off : kNvFragment;
		bool last = off + n == nvSize;
		OSWriteLittleInt32(msg, 0, kCtrlNvReq);
		OSWriteLittleInt32(msg, 4, 16 + n);
		OSWriteLittleInt16(msg, 8, (uint16_t)frag);
		OSWriteLittleInt16(msg, 10, last ? 1 : 0);
		OSWriteLittleInt32(msg, 12, n);
		memcpy(msg + 16, nv + off, n);

		int err;
		for (int tries = 0; (err = fCtrl->send(msg, 16 + n)) == -2 && tries < 1000; tries++) {
			fCtrl->poll();
			IOSleep(1);
		}
		ok = err == 0;
	}
	IOLog("PDQcomWCNSS: nv %u bytes %s\n", nvSize, ok ? "sent" : "send failed");
	if (nv != NULL)
		IOFreeData(nv, nvSize);
	IOFreeData(msg, 16 + kNvFragment);
	return ok;
}

bool
PDQcomWCNSS::startCtrl(void)
{
	if (fApcsMap == NULL && !openRpm())
		return false;
	volatile uint32_t *ipc = fIpc;

	// pronto creates its channels once it runs, so the lookup may need a moment
	for (int t = 0; t < 2000 && fCtrl == NULL; t += 10) {
		fCtrl = PDQcomSMDChannel::open("WCNSS_CTRL", kProntoEdge, kProntoHost, ipc, kProntoIpcBit, ctrlRecv, this);
		if (fCtrl == NULL)
			IOSleep(10);
	}
	if (fCtrl == NULL) {
		IOLog("PDQcomWCNSS: no WCNSS_CTRL channel\n");
		return false;
	}

	int t;
	for (t = 0; t < 2000 && !fCtrl->isOpen(); t++) {
		fCtrl->poll();
		IOSleep(1);
	}
	if (!fCtrl->isOpen()) {
		IOLog("PDQcomWCNSS: WCNSS_CTRL did not open\n");
		return false;
	}

	uint8_t req[8];
	OSWriteLittleInt32(req, 0, kCtrlVersionReq);
	OSWriteLittleInt32(req, 4, sizeof(req));
	fCtrl->send(req, sizeof(req));
	for (t = 0; t < 2000 && fCtrlState < 1; t++) {
		fCtrl->poll();
		IOSleep(1);
	}
	if (fCtrlState < 1) {
		IOLog("PDQcomWCNSS: no version reply\n");
		return false;
	}

	fCtrlState = 2;
	if (!sendNv())
		return false;
	for (t = 0; t < 5000 && fCtrlState < 3; t++) {
		fCtrl->poll();
		IOSleep(1);
	}
	return fCtrlState == 3;
}

// smem item 85 holds one smsm state word per host, apps first
// source: smem-before/after.bin of pd-prontoboot-pmu-transition and pd-pas-smem-film, bit 9 set by hal_start
enum {
	kSmsmSharedState = 85,
	kMaxChannels = 8,
};

static PDQcomWCNSS *sWcnss;

extern "C" void *
pd_wcnss_open_channel(const char *name, PDQcomSMDRecv recv, void *ctx)
{
	PDQcomWCNSS *w = sWcnss;

	return w != NULL ? w->openChannel(name, recv, ctx, false) : NULL;
}

// the caller polls this one itself with pd_wcnss_poll, so its callback runs in the caller's thread
extern "C" void *
pd_wcnss_open_channel_polled(const char *name, PDQcomSMDRecv recv, void *ctx)
{
	PDQcomWCNSS *w = sWcnss;

	return w != NULL ? w->openChannel(name, recv, ctx, true) : NULL;
}

extern "C" void
pd_wcnss_poll(void *ch)
{
	PDQcomWCNSS *w = sWcnss;

	if (w != NULL)
		w->pollChannel((PDQcomSMDChannel *)ch);
}

extern "C" int
pd_wcnss_send(void *ch, const void *data, uint32_t len)
{
	PDQcomWCNSS *w = sWcnss;

	return w != NULL ? w->sendChannel((PDQcomSMDChannel *)ch, data, len) : -1;
}

extern "C" void
pd_wcnss_close_channel(void *ch)
{
	PDQcomWCNSS *w = sWcnss;

	if (w != NULL)
		w->closeChannel((PDQcomSMDChannel *)ch);
}

extern "C" void
pd_wcnss_smsm_update(uint32_t mask, uint32_t value)
{
	uint32_t size = 0;
	volatile uint32_t *state = (volatile uint32_t *)PDQcomSMEM::get(-1, kSmsmSharedState, &size);

	if (state == NULL || size < 4)
		return;
	state[0] = (state[0] & ~mask) | (value & mask);
	__asm__ volatile ("dsb sy" ::: "memory");

	// pronto learns of the change through an ipc interrupt, the smsm node's qcom,ipc-3 (apcs bit 19)
	PDQcomWCNSS *w = sWcnss;
	if (w != NULL && w->fIpc != NULL)
		*w->fIpc = 1u << 19;
}

void *
PDQcomWCNSS::openChannel(const char *name, PDQcomSMDRecv recv, void *ctx, bool selfPolled)
{
	PDQcomSMDChannel *ch = NULL;

	IOLockLock(fLock);
	if (selfPolled) {
		ch = PDQcomSMDChannel::open(name, kProntoEdge, kProntoHost, fIpc, kProntoIpcBit, recv, ctx);
	} else {
		for (int i = 0; i < kMaxChannels && ch == NULL; i++) {
			if (fChannels[i] != NULL)
				continue;
			ch = PDQcomSMDChannel::open(name, kProntoEdge, kProntoHost, fIpc, kProntoIpcBit, recv, ctx);
			fChannels[i] = ch;
			break;
		}
	}
	IOLockUnlock(fLock);

	// the caller expects an open channel, as rpmsg endpoints are
	for (int t = 0; ch != NULL && t < 2000 && !ch->isOpen(); t++) {
		if (selfPolled)
			pollChannel(ch);
		IOSleep(1);
	}
	if (ch != NULL && !ch->isOpen()) {
		IOLog("PDQcomWCNSS: %s did not open\n", name);
		closeChannel(ch);
		ch = NULL;
	}
	return ch;
}

void
PDQcomWCNSS::pollChannel(PDQcomSMDChannel *ch)
{
	IOLockLock(fLock);
	ch->poll();
	IOLockUnlock(fLock);
}

int
PDQcomWCNSS::sendChannel(PDQcomSMDChannel *ch, const void *data, uint32_t len)
{
	int err;

	IOLockLock(fLock);
	err = ch->send(data, len);
	IOLockUnlock(fLock);
	return err;
}

void
PDQcomWCNSS::closeChannel(PDQcomSMDChannel *ch)
{
	IOLockLock(fLock);
	for (int i = 0; i < kMaxChannels; i++)
		if (fChannels[i] == ch)
			fChannels[i] = NULL;
	ch->close();
	IOLockUnlock(fLock);
}

// the smd interrupt is not wired up yet, so every channel is polled each millisecond
void
PDQcomWCNSS::pollThread(void *arg, wait_result_t wr)
{
	PDQcomWCNSS *self = (PDQcomWCNSS *)arg;

	(void)wr;
	while (!self->fStopping) {
		IOLockLock(self->fLock);
		for (int i = 0; i < kMaxChannels; i++)
			if (self->fChannels[i] != NULL)
				self->fChannels[i]->poll();
		IOLockUnlock(self->fLock);
		IOSleep(1);
	}
	thread_terminate(current_thread());
}

// the wifi node sits too deep for the platform expert: this service stands in for it, carrying its
// interrupts and its device tree path for the wlan driver that matches on PDQcomWCNSS
void
PDQcomWCNSS::publishWifi(void)
{
	IORegistryEntry *wifi = findCompatible("qcom,wcnss-wlan");
	char path[256];
	int len = sizeof(path);

	if (wifi == NULL || !wifi->getPath(path, &len, gIODTPlane)) {
		IOLog("PDQcomWCNSS: no wifi node\n");
		OSSafeReleaseNULL(wifi);
		return;
	}
	if (wifi->getProperty(gIOInterruptSpecifiersKey) == NULL)
		IODTMapInterrupts(wifi);
	OSObject *specs = wifi->getProperty(gIOInterruptSpecifiersKey);
	OSObject *ctrls = wifi->getProperty(gIOInterruptControllersKey);
	if (specs != NULL && ctrls != NULL) {
		setProperty(gIOInterruptSpecifiersKey, specs);
		setProperty(gIOInterruptControllersKey, ctrls);
	}
	setProperty("pd,dt-path", path);
	setProperty("pd,dt-compatible", "qcom,wcnss-wlan");
	IOLog("PDQcomWCNSS: wifi at %s, interrupts %s\n", path, specs != NULL ? "mapped" : "missing");
	OSSafeReleaseNULL(wifi);
}

void
PDQcomWCNSS::rpmRecv(void *ctx, const uint8_t *data, uint32_t len)
{
	PDQcomWCNSS *self = (PDQcomWCNSS *)ctx;

	(void)data;
	(void)len;
	self->fRpmAcks++;
}

// sends each vote and waits for the rpm to answer it before the next
bool
PDQcomWCNSS::rpmVotes(const void *votes, unsigned count)
{
	const RpmVote *v = (const RpmVote *)votes;

	for (unsigned i = 0; i < count; i++) {
		uint8_t msg[28 + 12 * 3];
		uint32_t kvpLen = 12 * v[i].kvps;
		OSWriteLittleInt32(msg, 0, kRpmReqMagic);
		OSWriteLittleInt32(msg, 4, 20 + kvpLen);
		OSWriteLittleInt32(msg, 8, ++fRpmId);
		OSWriteLittleInt32(msg, 12, v[i].set);
		OSWriteLittleInt32(msg, 16, v[i].type);
		OSWriteLittleInt32(msg, 20, v[i].id);
		OSWriteLittleInt32(msg, 24, kvpLen);
		for (unsigned k = 0; k < v[i].kvps; k++) {
			OSWriteLittleInt32(msg, 28 + 12 * k, v[i].key[k]);
			OSWriteLittleInt32(msg, 32 + 12 * k, 4);
			OSWriteLittleInt32(msg, 36 + 12 * k, v[i].value[k]);
		}

		uint32_t acks = fRpmAcks;
		int err = fRpm->send(msg, 28 + kvpLen);
		for (int t = 0; err == 0 && t < 200 && fRpmAcks == acks; t++) {
			fRpm->poll();
			IOSleep(1);
		}
		if (err != 0 || fRpmAcks == acks) {
			IOLog("PDQcomWCNSS: rpm vote %u (0x%08x %u) got no answer (%d)\n", i, v[i].type, v[i].id, err);
			return false;
		}
	}
	return true;
}

bool
PDQcomWCNSS::openRpm(void)
{
	if (!PDQcomSMEM::attach(kSmemBase, kSmemSize) || !PDQcomSMEM::attachAux(kRpmMsgRam, kRpmMsgRamSize))
		return false;
	fApcsMap = mapPhys(kApcsBase, 0x1000);
	if (fApcsMap == NULL)
		return false;
	fIpc = (volatile uint32_t *)(fApcsMap->getVirtualAddress() + kApcsIpc);

	fRpm = PDQcomSMDChannel::open("rpm_requests", kRpmEdge, -1, fIpc, kRpmIpcBit, rpmRecv, this);
	for (int t = 0; fRpm != NULL && t < 1000 && !fRpm->isOpen(); t++) {
		fRpm->poll();
		IOSleep(1);
	}
	IOLog("PDQcomWCNSS: rpm_requests %s\n", fRpm == NULL ? "missing" : fRpm->isOpen() ? "open" : "did not open");
	return fRpm != NULL && fRpm->isOpen();
}

void
PDQcomWCNSS::bringUp(void)
{
	// pdwcnssdelay=<seconds> lets usb and adb come up before anything here touches the hardware
	uint32_t delay = 30;
	PE_parse_boot_argn("pdwcnssdelay", &delay, sizeof(delay));
	IOSleep(delay * 1000);

	// the pronto node with the pmu block the progress word lives in
	IORegistryEntry *node = findCompatible("qcom,pronto-v2-pil");
	IOLog("PDQcomWCNSS: pronto node %s\n", node != NULL ? "found" : "missing");
	if (node == NULL)
		return;
	node->release();
	fPmuMap = mapPhys(kPmuBase, kPmuSize);
	if (fPmuMap == NULL)
		return;
	fPmu = (volatile uint32_t *)fPmuMap->getVirtualAddress();
	// no pmu access before auth_and_reset: reading it unclocked stopped the boot at stage 1
	// source: led stage 1 held on the uz801 with the read here, 2026-10-29

	// the iris data pins, gpio 40 to 44, to wcss_wlan: config 0x01 before wcnss starts and 0x85 after
	// (wcss-wlan-default-state, and openstick rpmfilm pins-before.txt and pins-after.txt register dumps)
	IOMemoryMap *pins = mapPhys(0x01028000, 5 * 0x1000);
	if (pins != NULL) {
		volatile uint8_t *base = (volatile uint8_t *)pins->getVirtualAddress();
		for (int g = 0; g < 5; g++)
			*(volatile uint32_t *)(base + g * 0x1000) = 0x85;
		__asm__ volatile ("dsb sy" ::: "memory");
		IOLog("PDQcomWCNSS: iris pins 0x%08x\n", *(volatile uint32_t *)base);
		pins->release();
	}

	// pronto and the iris radio need these rails and rf_clk2 while they come up
	bool votes = openRpm() && rpmVotes(kVotesOn, sizeof(kVotesOn) / sizeof(kVotesOn[0]));
	IOLog("PDQcomWCNSS: rpm votes %s\n", votes ? "on" : "failed");
	bool up = loadFirmware() && waitProgress(0x2a, 2000);
	// these votes stay, dropping them waits on pronto's handover signal, which needs smp2p
	(void)kVotesOff;
	if (up)

	setProperty("pronto-running", up);
	if (!up)
		return;

	bool ctrl = startCtrl();
	setProperty("wcnss-ctrl-ready", ctrl);
	IOLog("PDQcomWCNSS: control channel %s, progress 0x%08x\n", ctrl ? "ready" : "failed", fPmu[kPmuProgress / 4]);

	if (!ctrl)
		return;

	thread_t th;
	fChannels[0] = fCtrl;
	if (kernel_thread_start(&PDQcomWCNSS::pollThread, this, &th) == KERN_SUCCESS)
		thread_deallocate(th);
	sWcnss = this;
	publishWifi();
	registerService();
}

void
PDQcomWCNSS::bringUpThread(void *arg, wait_result_t wr)
{
	(void)wr;
	((PDQcomWCNSS *)arg)->bringUp();
	thread_terminate(current_thread());
}

extern "C" void pd_dump_threads(void);

// pdheartbeat=1 logs every 5 s with the wake-up lateness, to tell a starved kernel from a stuck userland
static void
heartbeatThread(void *arg, wait_result_t wr)
{
	(void)arg;
	(void)wr;
	for (uint32_t n = 0;; n++) {
		uint64_t t0, t1, ns;
		clock_get_uptime(&t0);
		IOSleep(5000);
		clock_get_uptime(&t1);
		absolutetime_to_nanoseconds(t1 - t0, &ns);
		IOLog("PD-BEAT %u late %llu ms\n", n, ns / 1000000 - 5000);
		// from a minute in, every 30 s: where each user thread is blocked
		if (n >= 12 && n % 6 == 0)
			pd_dump_threads();
	}
}

bool
PDQcomWCNSS::start(IOService *provider)
{
	uint32_t enable = 0, beat = 0;
	thread_t th;

	if (!IOService::start(provider))
		return false;

	PE_parse_boot_argn("pdheartbeat", &beat, sizeof(beat));
	if (beat && kernel_thread_start(heartbeatThread, NULL, &th) == KERN_SUCCESS)
		thread_deallocate(th);

	// off unless asked for while the port is young
	PE_parse_boot_argn("pdwcnss", &enable, sizeof(enable));
	if (!enable)
		return false;

	fLock = IOLockAlloc();
	if (kernel_thread_start(&PDQcomWCNSS::bringUpThread, this, &th) != KERN_SUCCESS)
		return false;
	thread_deallocate(th);
	return true;
}

void
PDQcomWCNSS::stop(IOService *provider)
{
	sWcnss = NULL;
	fStopping = true;
	IOSleep(10);
	for (int i = 0; i < kMaxChannels; i++)
		if (fChannels[i] != NULL)
			fChannels[i]->close();
	fCtrl = NULL;
	OSSafeReleaseNULL(fApcsMap);
	OSSafeReleaseNULL(fPmuMap);
	IOService::stop(provider);
}
