// SPDX-License-Identifier: MIT
// cdc-acm console function
#include "PDGadgetACM.h"

#include <IOKit/IOLib.h>
#include <kern/clock.h>
#include <sys/msgbuf.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <pexpert/pexpert.h>
#include <IOKit/IOPlatformExpert.h>

#define super PDGadgetFunction
OSDefineMetaClassAndStructors(PDGadgetACM, PDGadgetFunction);

extern "C" {
extern void (*PE_console_mirror_putc)(char c);
extern struct msgbuf *msgbufp;
void cons_cinput(char ch);
int pd_serial_line_feed(int chr);
boolean_t ml_set_interrupts_enabled(boolean_t enable);
}

// console bytes waiting for the host, the producer runs in any context, so both sides
// work with interrupts off (the RV1103 has one core)
#define RING_SIZE 65536u
static char sRing[RING_SIZE];
static uint32_t sHead, sTail;

static void
ringPut(char c)
{
	boolean_t s = ml_set_interrupts_enabled(FALSE);

	if (sHead - sTail < RING_SIZE) {
		sRing[sHead++ % RING_SIZE] = c;
	}
	ml_set_interrupts_enabled(s);
}

static uint32_t
ringTake(uint8_t *out, uint32_t max)
{
	boolean_t s = ml_set_interrupts_enabled(FALSE);
	uint32_t n = 0;

	while (n < max && sTail != sHead) {
		out[n++] = sRing[sTail++ % RING_SIZE];
	}
	ml_set_interrupts_enabled(s);
	return n;
}

PDGadgetFunction::Endpoint
PDGadgetACM::endpoint(uint8_t index)
{
	Endpoint e = {};

	switch (index) {
	case kNotify:
		e.type = kPDEpInterrupt;
		e.in = true;
		e.maxPacket = 16;
		e.interval = 9;		// 2^(9-1) microframes, 32 ms
		e.bufferSize = 64;
		break;
	case kDataIn:
		e.type = kPDEpBulk;
		e.in = true;
		e.maxPacket = 512;
		e.bufferSize = 4096;
		break;
	default:
		e.type = kPDEpBulk;
		e.in = false;
		e.maxPacket = 512;
		e.bufferSize = 512;
		break;
	}
	return e;
}

uint32_t
PDGadgetACM::descriptors(uint8_t *out, uint8_t firstInterface, const uint8_t *epAddr, bool highSpeed)
{
	uint8_t comm = firstInterface, data = firstInterface + 1;
	uint16_t bulk = highSpeed ? 512 : 64;
	const uint8_t d[] = {
		8, 0x0b, comm, 2, 0x02, 0x02, 0x01, 0,			// interface association
		9, 0x04, comm, 0, 1, 0x02, 0x02, 0x01, 0,		// communications: ACM, AT commands
		5, 0x24, 0x00, 0x10, 0x01,				// header, CDC 1.10
		5, 0x24, 0x01, 0x00, data,				// call management
		4, 0x24, 0x02, 0x06,					// ACM: line coding and state, break
		5, 0x24, 0x06, comm, data,				// union
		7, 0x05, epAddr[kNotify], 0x03, 16, 0, (uint8_t)(highSpeed ? 9 : 32),
		9, 0x04, data, 0, 2, 0x0a, 0x00, 0x00, 0,		// data
		7, 0x05, epAddr[kDataIn], 0x02, (uint8_t)(bulk & 0xff), (uint8_t)(bulk >> 8), 0,
		7, 0x05, epAddr[kDataOut], 0x02, (uint8_t)(bulk & 0xff), (uint8_t)(bulk >> 8), 0,
	};

	bcopy(d, out, sizeof(d));
	return sizeof(d);
}

int
PDGadgetACM::setup(const PDUSBSetup *s, uint8_t *data)
{
	switch (s->bRequest) {
	case 0x20:	// SET_LINE_CODING: arrives in the data stage
		return 0;
	case 0x21:	// GET_LINE_CODING
		bcopy(fLineCoding, data, sizeof(fLineCoding));
		return sizeof(fLineCoding);
	case 0x22: {	// SET_CONTROL_LINE_STATE
		bool dtr = (s->wValue & 1) != 0;

		if (dtr && !fDTR) {
			clock_get_uptime(&fDTRTime);
		}
		fDTR = dtr;
		return 0;
	}
	case 0x23:	// SEND_BREAK: reboot into the boot ROM's USB download mode
		clock_get_uptime(&fBreakTime);
		fBreak = true;
		return 0;
	default:
		return -1;
	}
}

void
PDGadgetACM::setupData(const PDUSBSetup *s, const uint8_t *data, uint32_t length)
{
	if (s->bRequest == 0x20 && length >= sizeof(fLineCoding)) {
		bcopy(data, fLineCoding, sizeof(fLineCoding));
	}
}

void
PDGadgetACM::configured(PDGadgetController *usb, uint8_t firstEndpoint)
{
	(void)firstEndpoint;
	fUSB = usb;
	if (fLineCoding[0] == 0) {
		const uint8_t lc[7] = { 0x00, 0xc2, 0x01, 0x00, 0, 0, 8 };	// 115200 8N1

		bcopy(lc, fLineCoding, sizeof(lc));
	}
	fUSB->queue(this, kDataOut, 512);
}

void
PDGadgetACM::deconfigured(void)
{
	fDTR = false;
	fUSB = NULL;
}

// replay the kernel log (its newest half-ring's worth), then follow the console live
void
PDGadgetACM::startMirror(void)
{
	struct msgbuf *mb = msgbufp;

	if (mb != NULL && mb->msg_bufc != NULL && mb->msg_size > 0) {
		uint32_t size = mb->msg_size, start = mb->msg_bufx, skip = 0;

		if (size > RING_SIZE / 2) {
			skip = size - RING_SIZE / 2;
		}
		for (uint32_t i = skip; i < size; i++) {
			char c = mb->msg_bufc[(start + i) % size];

			if (c == 0) {
				continue;
			}
			if (c == '\n') {
				ringPut('\r');
			}
			ringPut(c);
		}
	}
	PE_console_mirror_putc = ringPut;
	fMirroring = true;
}

void
PDGadgetACM::transferDone(uint8_t index, uint32_t actual)
{
	if (index == kDataOut && fUSB != NULL) {
		const uint8_t *buf = fUSB->buffer(this, kDataOut);

		for (uint32_t i = 0; i < actual; i++) {
			// ^A lines are serial hid reports, everything else is console input
			if (!pd_serial_line_feed(buf[i])) cons_cinput((char)buf[i]);
		}
		fUSB->queue(this, kDataOut, 512);
	}
}

void
PDGadgetACM::rebootToMaskrom(void)
{
	IOLog("PDGadgetACM: break from the host, rebooting to Maskrom\n");
	if (fUSB != NULL) {
		fUSB->rebootToMaskrom();
	}
	PEHaltRestart(kPERestartCPU);
}

void
PDGadgetACM::poll(void)
{
	if (fUSB == NULL) {
		return;
	}
	if (fBreak) {
		uint64_t now, ns;

		// let the status stage reach the host first
		clock_get_uptime(&now);
		absolutetime_to_nanoseconds(now - fBreakTime, &ns);
		if (ns > 100000000ULL) {
			rebootToMaskrom();
		}
	}
	if (!fMirroring) {
		uint64_t now, ns;

		// give the host a moment to put its tty in raw mode, or it echoes the log back
		clock_get_uptime(&now);
		absolutetime_to_nanoseconds(now - fDTRTime, &ns);
		if (!fDTR || ns < 300000000ULL) {
			return;
		}
		startMirror();
	}
	if (!fUSB->busy(this, kDataIn)) {
		uint32_t n = ringTake(fUSB->buffer(this, kDataIn), 4096);

		if (n > 0) {
			fUSB->queue(this, kDataIn, n);
		}
	}
}
