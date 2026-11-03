#include "PDGadgetADB.h"

#include <IOKit/IOLib.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <miscfs/devfs/devfs.h>

#define super PDGadgetFunction
OSDefineMetaClassAndStructors(PDGadgetADB, PDGadgetFunction);

// One device node, one function instance
static PDGadgetADB *sADB;

static int
adbOpen(dev_t dev, int flags, int devtype, struct proc *p)
{
	return sADB != NULL ? 0 : ENXIO;
}

static int
adbClose(dev_t dev, int flags, int devtype, struct proc *p)
{
	return 0;
}

static int
adbRead(dev_t dev, struct uio *uio, int flags)
{
	return sADB != NULL ? sADB->devRead(uio, flags) : ENXIO;
}

static int
adbWrite(dev_t dev, struct uio *uio, int flags)
{
	return sADB != NULL ? sADB->devWrite(uio, flags) : ENXIO;
}

static int
adbSelect(dev_t dev, int which, void *wql, struct proc *p)
{
	return sADB != NULL ? sADB->devSelect(which, wql, p) : 1;
}

static struct cdevsw sADBDevsw = {
	.d_open = adbOpen,
	.d_close = adbClose,
	.d_read = adbRead,
	.d_write = adbWrite,
	.d_ioctl = eno_ioctl,
	.d_stop = eno_stop,
	.d_reset = eno_reset,
	.d_ttys = NULL,
	.d_select = adbSelect,
	.d_mmap = eno_mmap,
	.d_strategy = eno_strat,
	.d_reserved_1 = eno_getc,
	.d_reserved_2 = eno_putc,
	.d_type = 0,
};

bool
PDGadgetADB::init(void)
{
	if (!super::init()) {
		return false;
	}
	fLock = IOLockAlloc();
	if (fLock == NULL) {
		return false;
	}
	fMajor = cdevsw_add(-1, &sADBDevsw);
	if (fMajor < 0) {
		IOLog("PDGadgetADB: no character device major\n");
		return false;
	}
	sADB = this;
	publishNode();
	return true;
}

void
PDGadgetADB::publishNode(void)
{
	if (fNode == NULL) {
		fNode = devfs_make_node(makedev(fMajor, 0), DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0600, "adb");
	}
}

void
PDGadgetADB::free(void)
{
	if (sADB == this) {
		sADB = NULL;
	}
	if (fLock != NULL) {
		IOLockFree(fLock);
	}
	super::free();
}

PDGadgetFunction::Endpoint
PDGadgetADB::endpoint(uint8_t index)
{
	Endpoint e = {};

	e.type = kPDEpBulk;
	e.in = index == kDataIn;
	e.maxPacket = 512;
	e.bufferSize = kBufferSize;
	return e;
}

uint32_t
PDGadgetADB::descriptors(uint8_t *out, uint8_t firstInterface, const uint8_t *epAddr, bool highSpeed)
{
	uint16_t bulk = highSpeed ? 512 : 64;
	const uint8_t d[] = {
		9, 0x04, firstInterface, 0, 2, 0xff, 0x42, 0x01, 0,	// ADB interface
		7, 0x05, epAddr[kDataIn], 0x02, (uint8_t)(bulk & 0xff), (uint8_t)(bulk >> 8), 0,
		7, 0x05, epAddr[kDataOut], 0x02, (uint8_t)(bulk & 0xff), (uint8_t)(bulk >> 8), 0,
	};

	bcopy(d, out, sizeof(d));
	return sizeof(d);
}

void
PDGadgetADB::configured(PDGadgetController *usb, uint8_t firstEndpoint)
{
	(void)firstEndpoint;
	IOLockLock(fLock);
	fUSB = usb;
	fOnline = true;
	fOutLen = fOutOff = 0;
	fOutRequeue = false;
	fInPending = fInQueued = false;
	IOLockUnlock(fLock);
	fUSB->queue(this, kDataOut, kBufferSize);
}

void
PDGadgetADB::deconfigured(void)
{
	IOLockLock(fLock);
	fOnline = false;
	fUSB = NULL;
	fOutLen = fOutOff = 0;
	fInPending = fInQueued = false;
	IOLockWakeup(fLock, &fOutLen, false);
	IOLockWakeup(fLock, &fInPending, false);
	IOLockUnlock(fLock);
	selwakeup(&fReadSel);
	selwakeup(&fWriteSel);
}

void
PDGadgetADB::transferDone(uint8_t index, uint32_t actual)
{
	IOLockLock(fLock);
	if (index == kDataOut) {
		fOutLen = actual;
		fOutOff = 0;
		// A zero-length packet carries nothing: take the next one straight away
		fOutRequeue = actual == 0;
		IOLockWakeup(fLock, &fOutLen, false);
	} else {
		fInPending = fInQueued = false;
		IOLockWakeup(fLock, &fInPending, false);
	}
	IOLockUnlock(fLock);
	selwakeup(index == kDataOut ? &fReadSel : &fWriteSel);
}

// Endpoint work happens on the controller's thread, userland only moves the bytes
void
PDGadgetADB::poll(void)
{
	bool requeueOut, queueIn;
	uint32_t inLen;

	if (fUSB == NULL) {
		return;
	}
	publishNode();
	IOLockLock(fLock);
	requeueOut = fOnline && fOutRequeue;
	fOutRequeue = false;
	queueIn = fOnline && fInPending && !fInQueued;
	inLen = fInLen;
	if (queueIn) {
		fInQueued = true;
	}
	IOLockUnlock(fLock);
	if (requeueOut) {
		fUSB->queue(this, kDataOut, kBufferSize);
	}
	if (queueIn && !fUSB->queue(this, kDataIn, inLen)) {
		IOLockLock(fLock);
		fInQueued = false;
		IOLockUnlock(fLock);
	}
}

int
PDGadgetADB::devRead(struct uio *uio, int flags)
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
	if (fOutOff == fOutLen) {
		fOutRequeue = true;
	}
	IOLockUnlock(fLock);
	return error;
}

int
PDGadgetADB::devWrite(struct uio *uio, int flags)
{
	int error = 0;

	while (uio_resid(uio) > 0 && error == 0) {
		IOLockLock(fLock);
		while (fOnline && fInPending) {
			if (flags & IO_NDELAY) {
				IOLockUnlock(fLock);
				return EWOULDBLOCK;
			}
			if (IOLockSleep(fLock, &fInPending, THREAD_INTERRUPTIBLE) == THREAD_INTERRUPTED) {
				IOLockUnlock(fLock);
				return EINTR;
			}
		}
		if (!fOnline) {
			IOLockUnlock(fLock);
			return ENXIO;
		}
		uint32_t n = uio_resid(uio) > kBufferSize ? kBufferSize : (uint32_t)uio_resid(uio);

		error = uiomove((char *)fUSB->buffer(this, kDataIn), (int)n, uio);
		if (error == 0) {
			fInLen = n;
			fInPending = true;
		}
		IOLockUnlock(fLock);
	}
	return error;
}

int
PDGadgetADB::devSelect(int which, void *wql, struct proc *p)
{
	bool ready;

	IOLockLock(fLock);
	if (which == FREAD) {
		ready = !fOnline || fOutOff < fOutLen;
		if (!ready) {
			selrecord(p, &fReadSel, wql);
		}
	} else {
		ready = !fOnline || !fInPending;
		if (!ready) {
			selrecord(p, &fWriteSel, wql);
		}
	}
	IOLockUnlock(fLock);
	return ready ? 1 : 0;
}
