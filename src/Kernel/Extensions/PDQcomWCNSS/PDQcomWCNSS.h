// PDQcomWCNSS.h - msm8916 wcnss (pronto) loader
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef PDQCOMWCNSS_H
#define PDQCOMWCNSS_H

#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>
#include <kern/thread.h>

#include "PDQcomSMD.h"

class PDQcomWCNSS : public IOService
{
	OSDeclareDefaultStructors(PDQcomWCNSS);

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	void *openChannel(const char *name, PDQcomSMDRecv recv, void *ctx, bool selfPolled);
	void pollChannel(PDQcomSMDChannel *ch);
	int sendChannel(PDQcomSMDChannel *ch, const void *data, uint32_t len);
	void closeChannel(PDQcomSMDChannel *ch);

	volatile uint32_t *fIpc;

private:
	static void bringUpThread(void *arg, wait_result_t wr);
	void bringUp(void);
	bool readFile(const char *name, uint8_t **data, uint32_t *size, uint32_t limit);
	bool loadFirmware(void);
	bool waitProgress(uint8_t want, uint32_t ms);
	bool startCtrl(void);
	bool sendNv(void);
	static void ctrlRecv(void *ctx, const uint8_t *data, uint32_t len);
	static void pollThread(void *arg, wait_result_t wr);
	static void rpmRecv(void *ctx, const uint8_t *data, uint32_t len);
	bool openRpm(void);
	bool rpmVotes(const void *votes, unsigned count);
	void publishWifi(void);

	IOMemoryMap *fPmuMap;
	volatile uint32_t *fPmu;
	IOMemoryMap *fApcsMap;
	PDQcomSMDChannel *fCtrl;
	int fCtrlState;
	IOLock *fLock;
	PDQcomSMDChannel *fChannels[8];
	volatile bool fStopping;
	PDQcomSMDChannel *fRpm;
	volatile uint32_t fRpmAcks;
	uint32_t fRpmId;
};

#endif
