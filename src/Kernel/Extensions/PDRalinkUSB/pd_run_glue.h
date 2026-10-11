/*
 * pd_run_glue.h - run(4) as PDRalinkUSB sees it.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#ifndef PD_RUN_GLUE_H
#define PD_RUN_GLUE_H

#ifdef __cplusplus
extern "C" {
#endif

struct run_softc;
struct ieee80211com;
struct usbd_device;
struct usbd_interface;

struct run_softc *pd_run_alloc(struct usbd_device *, struct usbd_interface *, const char *);
void	pd_run_free(struct run_softc *);
int	pd_run_attach(struct run_softc *);
void	pd_run_detach(struct run_softc *);
struct ieee80211com *pd_run_ic(struct run_softc *);

#ifdef __cplusplus
}
#endif

#endif
