// SPDX-License-Identifier: MIT
// the wlan side of attach: once a linuxkpi driver registered its ieee80211com,
// one station vap is made so its ifnet can be handed to iokit

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/haiku-module.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>

struct ieee80211vap *pd_wlan_create_vap(struct ieee80211com *ic, int unit);
extern int (*pd_fbsd_post_attach)(device_t dev);
extern int pd_fbsd_frames_on_transmit;
extern void (*pd_fbsd_settle)(void *ifp);

static int sVapUnit;

static void
create_missing_vap(void *arg, struct ieee80211com *ic)
{
	int *made = arg;

	if (!TAILQ_EMPTY(&ic->ic_vaps))
		return;

	struct ieee80211vap *vap = pd_wlan_create_vap(ic, sVapUnit);
	if (vap == NULL) {
		ic_printf(ic, "could not create a station vap\n");
		return;
	}
	sVapUnit++;
	(*made)++;
	if_printf(vap->iv_ifp, "station vap on %s\n", ic->ic_name);
}

static int
lkpi_wlan_post_attach(device_t dev)
{
	int made = 0;

	ieee80211_iterate_coms(create_missing_vap, &made);
	return made > 0 ? 0 : ENXIO;
}

// a vap's queued state changes and its radio's up or down, finished before pd_fbsd_up brings it up again
static void
lkpi_wlan_settle(void *cookie)
{
	struct ifnet *ifp = cookie;
	struct ieee80211vap *vap = ifp->if_softc;
	struct ieee80211com *ic = vap->iv_ic;

	for (int i = 0; i < NET80211_IV_NSTATE_NUM; i++)
		ieee80211_draintask(ic, &vap->iv_nstate_task[i]);
	ieee80211_waitfor_parent(ic);
}

status_t
init_wlan_stack(void)
{
	pd_fbsd_settle = lkpi_wlan_settle;
	pd_fbsd_post_attach = lkpi_wlan_post_attach;
	pd_fbsd_frames_on_transmit = 1;
	return B_OK;
}

void
uninit_wlan_stack(void)
{
	pd_fbsd_post_attach = NULL;
}

status_t
start_wlan(device_t dev)
{
	return B_OK;
}

status_t
stop_wlan(device_t dev)
{
	return B_OK;
}

status_t
wlan_control(void *cookie, uint32 op, void *arg, size_t length)
{
	return B_BAD_VALUE;
}

status_t
wlan_close(void *cookie)
{
	return B_OK;
}
