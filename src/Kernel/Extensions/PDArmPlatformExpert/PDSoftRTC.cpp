#include "PDSoftRTC.h"

#include <IOKit/IOLib.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>

#define super IORTC
OSDefineMetaClassAndStructors(PDSoftRTC, IORTC);

// Seconds since 1970 for a civil date (proleptic Gregorian)
static uint64_t
pdDaysFromCivil(int y, unsigned m, unsigned d)
{
	y -= m <= 2;
	int era = (y >= 0 ? y : y - 399) / 400;
	unsigned yoe = (unsigned)(y - era * 400);
	unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

	return (uint64_t)(era * 146097 + (int)doe - 719468);
}

// When this kext was built: better than 1970 on a board that keeps no time
static uint64_t
pdBuildEpoch(void)
{
	static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	const char *date = __DATE__, *time = __TIME__;	// "Sep 24 2026", "23:42:20"
	unsigned m = 1;

	for (unsigned i = 0; i < 12; i++) {
		if (strncmp(date, months + i * 3, 3) == 0) {
			m = i + 1;
		}
	}
	int day = (date[4] == ' ' ? 0 : date[4] - '0') * 10 + (date[5] - '0');
	int year = (date[7] - '0') * 1000 + (date[8] - '0') * 100 + (date[9] - '0') * 10 + (date[10] - '0');
	int h = (time[0] - '0') * 10 + (time[1] - '0');
	int min = (time[3] - '0') * 10 + (time[4] - '0');
	int s = (time[6] - '0') * 10 + (time[7] - '0');

	return pdDaysFromCivil(year, m, (unsigned)day) * 86400 + (uint64_t)(h * 3600 + min * 60 + s);
}

bool
PDSoftRTC::start(IOService *provider)
{
	uint64_t epoch = 0;

	if (!super::start(provider)) {
		return false;
	}
	// pdtime=<seconds since 1970>: the host's clock, passed along by the loader
	if (!PE_parse_boot_argn("pdtime", &epoch, sizeof(epoch)) || epoch == 0) {
		epoch = pdBuildEpoch();
	}
	setGMTTimeOfDay((long)epoch);
	publishResource("IORTC", this);
	registerService();
	IOLog("PDSoftRTC: no clock on this board, starting from %llu\n", epoch);
	return true;
}

// The time set last, carried forward by the uptime clock, which does not stop
long
PDSoftRTC::getGMTTimeOfDay(void)
{
	clock_sec_t secs;
	clock_usec_t usecs;

	clock_get_system_microtime(&secs, &usecs);
	return (long)(fBase + (secs - fBaseUptime));
}

void
PDSoftRTC::setGMTTimeOfDay(long secs)
{
	clock_sec_t now;
	clock_usec_t usecs;

	clock_get_system_microtime(&now, &usecs);
	fBase = (uint64_t)secs;
	fBaseUptime = now;
}
