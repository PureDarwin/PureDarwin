/*
 * pd_run_glue.c - what PDRalinkUSB needs from run(4) without seeing its
 * softc: allocation, the usb handles, attach, detach and the 802.11 state.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>
#include "rt2860reg.h"
#include "pd_usbd.h"
#include "if_runvar.h"
#include "pd_run_glue.h"

int	run_attach(struct run_softc *);
int	run_detach(struct run_softc *);

// the firmware image linked into the kext, generated from Resources/rt2870.bin
extern const uint8_t pd_rt2870_fw[];
extern const size_t pd_rt2870_fw_len;

struct run_softc *
pd_run_alloc(struct usbd_device *udev, struct usbd_interface *iface, const char *name)
{
	struct run_softc *sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK | M_ZERO);

	if (sc == NULL)
		return NULL;
	sc->sc_udev = udev;
	sc->sc_iface = iface;
	strlcpy(sc->sc_dev.dv_xname, name, sizeof(sc->sc_dev.dv_xname));
	return sc;
}

void
pd_run_free(struct run_softc *sc)
{
	free(sc, M_DEVBUF, sizeof(*sc));
}

int
pd_run_attach(struct run_softc *sc)
{
	int error;

	pd80211_lock();
	error = run_attach(sc);
	pd80211_unlock();
	return error;
}

void
pd_run_detach(struct run_softc *sc)
{
	pd80211_lock();
	run_detach(sc);
	pd80211_unlock();
}

struct ieee80211com *
pd_run_ic(struct run_softc *sc)
{
	return &sc->sc_ic;
}

// run-rt2870 is the first 4KB of rt2870.bin, run-rt3071 the second
int
loadfirmware(const char *name, uint8_t **bufp, size_t *sizep)
{
	size_t off;
	uint8_t *buf;

	if (strcmp(name, "run-rt2870") == 0)
		off = 0;
	else if (strcmp(name, "run-rt3071") == 0)
		off = 4096;
	else
		return ENOENT;
	if (pd_rt2870_fw_len < off + 4096)
		return EINVAL;
	buf = malloc(4096, M_DEVBUF, M_WAITOK);
	if (buf == NULL)
		return ENOMEM;
	memcpy(buf, pd_rt2870_fw + off, 4096);
	*bufp = buf;
	*sizep = 4096;
	return 0;
}
