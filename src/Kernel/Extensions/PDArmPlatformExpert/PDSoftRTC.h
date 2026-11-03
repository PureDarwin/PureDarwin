#ifndef _PUREDARWIN_PDSOFTRTC_H
#define _PUREDARWIN_PDSOFTRTC_H

#include <IOKit/rtc/IORTCController.h>

/*
 * A clock for boards without one: starts from the host's time (pdtime= boot-arg) or the
 * build date, runs on the uptime clock, and keeps whatever settimeofday sets. Published
 * as the IORTC resource, which the platform expert answers time queries from.
 */
class PDSoftRTC : public IORTC
{
	OSDeclareDefaultStructors(PDSoftRTC);

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;

	long getGMTTimeOfDay(void) APPLE_KEXT_OVERRIDE;
	void setGMTTimeOfDay(long secs) APPLE_KEXT_OVERRIDE;
	void setAlarmEnable(IOOptionBits message) APPLE_KEXT_OVERRIDE {}

private:
	uint64_t fBase;
	uint64_t fBaseUptime;
};

#endif /* _PUREDARWIN_PDSOFTRTC_H */
