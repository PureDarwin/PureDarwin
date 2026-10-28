// SPDX-License-Identifier: MIT
// bulk pipe function and its /dev node, one minor per pipe
#include "PDGadgetPipe.h"

#include <IOKit/IOLib.h>
#include <kern/clock.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <miscfs/devfs/devfs.h>

#define super PDGadgetFunction
OSDefineMetaClassAndStructors(PDGadgetPipe, PDGadgetFunction);

// one character major for every pipe, the minor picks the pipe
static PDGadgetPipe *sPipes[PDGadgetPipe::kMaxPipes];
static int sMajor = -1;

static PDGadgetPipe *
pipeFor(dev_t dev)
{
	int m = minor(dev);

	return m >= 0 && m < PDGadgetPipe::kMaxPipes ? sPipes[m] : NULL;
}

static int
pipeOpen(dev_t dev, int flags, int devtype, struct proc *p)
{
	return pipeFor(dev) != NULL ? 0 : ENXIO;
}

static int
pipeClose(dev_t dev, int flags, int devtype, struct proc *p)
{
	return 0;
}

static int
pipeRead(dev_t dev, struct uio *uio, int flags)
{
	PDGadgetPipe *pp = pipeFor(dev);

	return pp != NULL ? pp->devRead(uio, flags) : ENXIO;
}

static int
pipeWrite(dev_t dev, struct uio *uio, int flags)
{
	PDGadgetPipe *pp = pipeFor(dev);

	return pp != NULL ? pp->devWrite(uio, flags) : ENXIO;
}

static int
pipeSelect(dev_t dev, int which, void *wql, struct proc *p)
{
	PDGadgetPipe *pp = pipeFor(dev);

	return pp != NULL ? pp->devSelect(which, wql, p) : 1;
}

static struct cdevsw sPipeDevsw = {
	.d_open = pipeOpen,
	.d_close = pipeClose,
	.d_read = pipeRead,
	.d_write = pipeWrite,
	.d_ioctl = eno_ioctl,
	.d_stop = eno_stop,
	.d_reset = eno_reset,
	.d_ttys = NULL,
	.d_select = pipeSelect,
	.d_mmap = eno_mmap,
	.d_strategy = eno_strat,
	.d_reserved_1 = eno_getc,
	.d_reserved_2 = eno_putc,
	.d_type = 0,
};

bool
PDGadgetPipe::initPipe(const char *node, uint8_t cls, uint8_t sub, uint8_t proto, uint32_t inSize)
{
	if (!super::init()) {
		return false;
	}
	fLock = IOLockAlloc();
	if (fLock == NULL) {
		return false;
	}
	if (sMajor < 0 && (sMajor = cdevsw_add(-1, &sPipeDevsw)) < 0) {
		IOLog("PDGadgetPipe: no character device major\n");
		return false;
	}
	fMinor = -1;
	for (int i = 0; i < kMaxPipes; i++) {
		if (sPipes[i] == NULL) {
			fMinor = i;
			break;
		}
	}
	if (fMinor < 0) {
		return false;
	}
	strlcpy(fName, node, sizeof(fName));
	fInSize = inSize;
	fClass = cls;
	fSubClass = sub;
	fProtocol = proto;
	fMajor = sMajor;
	sPipes[fMinor] = this;
	publishNode();
	return true;
}

void
PDGadgetPipe::publishNode(void)
{
	if (fNode == NULL && mach_absolute_time() >= fNextPublish) {
		uint64_t second;

		fNode = devfs_make_node(makedev(fMajor, fMinor), DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0600, "%s", fName);
		nanoseconds_to_absolutetime(NSEC_PER_SEC, &second);
		fNextPublish = mach_absolute_time() + second;
	}
}

void
PDGadgetPipe::free(void)
{
	if (fMinor >= 0 && fMinor < kMaxPipes && sPipes[fMinor] == this) {
		sPipes[fMinor] = NULL;
	}
	if (fLock != NULL) {
		IOLockFree(fLock);
	}
	super::free();
}

void
PDGadgetPipe::resetSlots(void)
{
	for (int i = 0; i < kInSlots; i++) {
		fSlot[i] = kFree;
		fSlotLen[i] = 0;
	}
	fNextFill = fNextSend = 0;
}

PDGadgetFunction::Endpoint
PDGadgetPipe::endpoint(uint8_t index)
{
	Endpoint e = {};

	e.type = kPDEpBulk;
	e.in = index == kDataIn;
	e.maxPacket = 512;
	e.bufferSize = e.in ? fInSize * kInSlots : kBufferSize;
	return e;
}

uint32_t
PDGadgetPipe::descriptors(uint8_t *out, uint8_t firstInterface, const uint8_t *epAddr, bool highSpeed)
{
	uint16_t bulk = highSpeed ? 512 : 64;
	const uint8_t d[] = {
		9, 0x04, firstInterface, 0, 2, fClass, fSubClass, fProtocol, 0,
		7, 0x05, epAddr[kDataIn], 0x02, (uint8_t)(bulk & 0xff), (uint8_t)(bulk >> 8), 0,
		7, 0x05, epAddr[kDataOut], 0x02, (uint8_t)(bulk & 0xff), (uint8_t)(bulk >> 8), 0,
	};

	bcopy(d, out, sizeof(d));
	return sizeof(d);
}

void
PDGadgetPipe::configured(PDGadgetController *usb, uint8_t firstEndpoint)
{
	(void)firstEndpoint;
	IOLockLock(fLock);
	fUSB = usb;
	fOnline = true;
	fOutLen = fOutOff = 0;
	fOutRequeue = false;
	resetSlots();
	IOLockUnlock(fLock);
	fUSB->queue(this, kDataOut, kBufferSize);
}

void
PDGadgetPipe::deconfigured(void)
{
	IOLockLock(fLock);
	fOnline = false;
	fUSB = NULL;
	fOutLen = fOutOff = 0;
	resetSlots();
	IOLockWakeup(fLock, &fOutLen, false);
	IOLockWakeup(fLock, fSlot, false);
	IOLockUnlock(fLock);
	selwakeup(&fReadSel);
	selwakeup(&fWriteSel);
}

void
PDGadgetPipe::transferDone(uint8_t index, uint32_t actual)
{
	IOLockLock(fLock);
	if (index == kDataOut) {
		fOutLen = actual;
		fOutOff = 0;
		// A zero-length packet carries nothing: take the next one straight away
		fOutRequeue = actual == 0;
		IOLockWakeup(fLock, &fOutLen, false);
	} else if (fSlot[fNextSend] == kInFlight) {
		fSlot[fNextSend] = kFree;
		fNextSend = (fNextSend + 1) % kInSlots;
		IOLockWakeup(fLock, fSlot, false);
	}
	IOLockUnlock(fLock);
	selwakeup(index == kDataOut ? &fReadSel : &fWriteSel);
}

// endpoint work happens on the controller's thread, userland only moves the bytes
void
PDGadgetPipe::poll(void)
{
	bool requeueOut, queueIn;
	uint32_t inLen, inSlot;

	if (fUSB == NULL) {
		return;
	}
	publishNode();
	IOLockLock(fLock);
	requeueOut = fOnline && fOutRequeue;
	fOutRequeue = false;
	// one transfer at a time on the endpoint: the next filled slot goes when the bus is free
	inSlot = fNextSend;
	queueIn = fOnline && fSlot[inSlot] == kFilled && !fUSB->busy(this, kDataIn);
	inLen = fSlotLen[inSlot];
	if (queueIn) {
		fSlot[inSlot] = kInFlight;
	}
	IOLockUnlock(fLock);
	if (requeueOut) {
		fUSB->queue(this, kDataOut, kBufferSize);
	}
	if (queueIn && !fUSB->queueAt(this, kDataIn, inSlot * fInSize, inLen)) {
		IOLockLock(fLock);
		fSlot[inSlot] = kFilled;
		IOLockUnlock(fLock);
	}
}

int
PDGadgetPipe::devRead(struct uio *uio, int flags)
{
	int error = 0;

	IOLockLock(fLock);
	while (fOnline && fOutOff == fOutLen) {
		if (flags & IO_NDELAY) {
			IOLockUnlock(fLock);
			return EWOULDBLOCK;
		}
		if (IOLockSleep(fLock, &fOutLen, THREAD_INTERRUPTIBLE) == THREAD_INTERRUPTED) {
			IOLockUnlock(fLock);
			return EINTR;
		}
	}
	if (!fOnline) {
		IOLockUnlock(fLock);
		return ENXIO;
	}
	uint32_t n = fOutLen - fOutOff;

	if (n > uio_resid(uio)) {
		n = (uint32_t)uio_resid(uio);
	}
	error = uiomove((const char *)fUSB->buffer(this, kDataOut) + fOutOff, (int)n, uio);
	fOutOff += n;
	bool drained = fOutOff == fOutLen;
	if (drained) {
		fOutRequeue = true;
	}
	PDGadgetController *usb = fUSB;
	IOLockUnlock(fLock);
	if (drained && usb != NULL) {
		usb->kick();
	}
	return error;
}

int
PDGadgetPipe::devWrite(struct uio *uio, int flags)
{
	int error = 0;

	while (uio_resid(uio) > 0 && error == 0) {
		IOLockLock(fLock);
		while (fOnline && fSlot[fNextFill] != kFree) {
			if (flags & IO_NDELAY) {
				IOLockUnlock(fLock);
				return EWOULDBLOCK;
			}
			if (IOLockSleep(fLock, fSlot, THREAD_INTERRUPTIBLE) == THREAD_INTERRUPTED) {
				IOLockUnlock(fLock);
				return EINTR;
			}
		}
		if (!fOnline) {
			IOLockUnlock(fLock);
			return ENXIO;
		}
		uint32_t n = uio_resid(uio) > fInSize ? fInSize : (uint32_t)uio_resid(uio);

		uint8_t slot = fNextFill;

		error = uiomove((char *)fUSB->buffer(this, kDataIn) + slot * fInSize, (int)n, uio);
		if (error == 0) {
			fSlotLen[slot] = n;
			fSlot[slot] = kFilled;
			fNextFill = (slot + 1) % kInSlots;
		}
		PDGadgetController *usb = fUSB;
		IOLockUnlock(fLock);
		if (error == 0 && usb != NULL) {
			usb->kick();
		}
	}
	return error;
}

int
PDGadgetPipe::devSelect(int which, void *wql, struct proc *p)
{
	bool ready;

	IOLockLock(fLock);
	if (which == FREAD) {
		ready = !fOnline || fOutOff < fOutLen;
		if (!ready) {
			selrecord(p, &fReadSel, wql);
		}
	} else {
		ready = !fOnline || fSlot[fNextFill] == kFree;
		if (!ready) {
			selrecord(p, &fWriteSel, wql);
		}
	}
	IOLockUnlock(fLock);
	return ready ? 1 : 0;
}
