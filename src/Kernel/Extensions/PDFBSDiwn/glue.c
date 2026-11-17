// SPDX-License-Identifier: MIT
// iwn's pci attachment on fbsdkpi, the radio sits on net80211

#include <sys/bus.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_ratectl.h>

#include <dev/iwn/if_iwnreg.h>
#include <dev/iwn/if_iwnvar.h>

HAIKU_FBSD_WLAN_DRIVER_GLUE(iwn, iwn, pci)
HAIKU_DRIVER_REQUIREMENTS(FBSD_TASKQUEUES | FBSD_WLAN);
NO_HAIKU_FBSD_MII_DRIVER();
NO_HAIKU_REENABLE_INTERRUPTS();

// masks the card and says whether it raised the interrupt, iwn_intr reads and acks the cause itself
int
HAIKU_CHECK_DISABLE_INTERRUPTS(device_t dev)
{
	struct iwn_softc *sc = device_get_softc(dev);
	uint32_t r1, r2;

	IWN_WRITE(sc, IWN_INT_MASK, 0);
	r1 = IWN_READ(sc, IWN_INT);
	// all ones or the a5a5 pattern is a card that is gone or asleep
	if (r1 == 0xffffffff || (r1 & 0xfffffff0) == 0xa5a5a5a0)
		return 0;
	r2 = IWN_READ(sc, IWN_FH_INT);
	if (r1 == 0 && r2 == 0) {
		if (sc->sc_flags & IWN_FLAG_RUNNING)
			IWN_WRITE(sc, IWN_INT_MASK, sc->int_mask);
		return 0;
	}
	return 1;
}
