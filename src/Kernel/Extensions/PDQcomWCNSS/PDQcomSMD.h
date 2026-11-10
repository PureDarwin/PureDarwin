// PDQcomSMD.h - qualcomm smd stream channels over smem, from lk2nd platform/msm_shared/smd.c
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PDQCOMSMD_H
#define PDQCOMSMD_H

#include <stdint.h>
#include <stddef.h>

struct PDQcomSMDPort;

typedef void (*PDQcomSMDRecv)(void *ctx, const uint8_t *data, uint32_t len);

class PDQcomSMDChannel
{
public:
	static PDQcomSMDChannel *open(const char *name, uint32_t edge, int host, volatile uint32_t *ipc,
	    uint32_t ipcBit, PDQcomSMDRecv recv, void *ctx);
	int send(const void *data, uint32_t len);
	void poll(void);
	bool isOpen(void);
	void close(void);

	char fName[20];

private:
	void kick(void);
	void setState(uint32_t state);
	uint32_t avail(void);
	void readFifo(uint8_t *dst, uint32_t len);
	void writeFifo(const uint8_t *src, uint32_t len);

	PDQcomSMDPort *fPort;
	uint8_t *fTx, *fRx;
	uint32_t fFifoSize;
	uint32_t fState;
	volatile uint32_t *fIpc;
	uint32_t fIpcBit;
	PDQcomSMDRecv fRecv;
	void *fCtx;
	uint8_t *fPkt;
	uint32_t fPktSize;
};

#endif
