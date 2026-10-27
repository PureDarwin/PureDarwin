// surface_host.h - pd_surface_host_v1, a surface one client draws shown as a subsurface of another client's surface.
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef SWC_SURFACE_HOST_H
#define SWC_SURFACE_HOST_H

#include <stdbool.h>

struct wl_display;

// surfaces one client exports and another shows as its own subsurface (CAContext and CALayerHost)
struct wl_global *
surface_host_create(struct wl_display *display);

bool surface_host_trace_on(void);

#endif
