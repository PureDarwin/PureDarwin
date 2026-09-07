/* swc: libswc/swc.c
 *
 * Copyright (c) 2013-2020 Michael Forney
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "swc.h"
#include "bindings.h"
#include "compositor.h"
#include "data_device_manager.h"
#ifdef ENABLE_DRM
#include "drm.h"
#else
#include "fb.h"
#endif
#include "event.h"
#include "internal.h"
#include "kde_decoration.h"
#include "keyboard.h"
#include "launch.h"
#include "layer_shell.h"
#include "panel_manager.h"
#include "pointer.h"
#include "screen.h"
#include "seat.h"
#include "select.h"
#include "shell.h"
#include "shm.h"
#include "snap.h"
#include "subcompositor.h"
#include "util.h"
#include "window.h"
#include "xdg_decoration.h"
#include "xdg_output.h"
#include "xdg_shell.h"
#ifdef ENABLE_XWAYLAND
#include "xserver.h"
#endif

extern struct swc_launch swc_launch;
extern const struct swc_bindings swc_bindings;
extern struct swc_compositor swc_compositor;
#ifdef ENABLE_DRM
extern struct swc_drm swc_drm;
#endif
#ifdef ENABLE_XWAYLAND
extern struct swc_xserver swc_xserver;
#endif

extern struct pointer_handler screens_pointer_handler;

struct swc swc = {
    .bindings = &swc_bindings,
    .compositor = &swc_compositor,
#ifdef ENABLE_DRM
    .drm = &swc_drm,
#endif
#ifdef ENABLE_XWAYLAND
    .xserver = &swc_xserver,
#endif
};

static void
setup_compositor(void)
{
	pixman_region32_t pointer_region;
	struct screen *screen;
	struct swc_rectangle *geom;

	wl_list_insert(&swc.seat->keyboard->handlers,
	               &swc.bindings->keyboard_handler->link);
	wl_list_insert(&swc.seat->pointer->handlers,
	               &swc.bindings->pointer_handler->link);
	wl_list_insert(&swc.seat->pointer->handlers,
	               &swc.compositor->pointer_handler->link);
	wl_list_insert(&swc.seat->pointer->handlers, &screens_pointer_handler.link);
	wl_signal_add(&swc.seat->pointer->focus.event_signal,
	              &window_enter_listener);

	/* Calculate pointer region */
	pixman_region32_init(&pointer_region);

	wl_list_for_each(screen, &swc.screens, link)
	{
		geom = &screen->base.geometry;
		pixman_region32_union_rect(&pointer_region, &pointer_region, geom->x,
		                           geom->y, geom->width, geom->height);
	}

	pointer_set_region(swc.seat->pointer, &pointer_region);
	pixman_region32_fini(&pointer_region);
}

void
swc_activate(void)
{
	swc.active = true;
	send_event(&swc.event_signal, SWC_EVENT_ACTIVATED, NULL);
	if (swc.manager->activate) {
		swc.manager->activate();
	}
}

void
swc_deactivate(void)
{
	swc.active = false;
	send_event(&swc.event_signal, SWC_EVENT_DEACTIVATED, NULL);
	if (swc.manager->deactivate) {
		swc.manager->deactivate();
	}
}

EXPORT bool
swc_cursor_position(int32_t *x, int32_t *y)
{
	if (x) {
		*x = 0;
	}
	if (y) {
		*y = 0;
	}

	if (!swc.seat || !swc.seat->pointer) {
		return false;
	}

	if (x) {
		*x = swc.seat->pointer->x;
	}
	if (y) {
		*y = swc.seat->pointer->y;
	}

	return true;
}

EXPORT bool
swc_cursor_set_position(int32_t x, int32_t y)
{
	if (!swc.seat || !swc.seat->pointer) {
		return false;
	}

	pointer_handle_absolute_motion(swc.seat->pointer, get_time(),
	                               wl_fixed_from_int(x), wl_fixed_from_int(y));
	pointer_handle_frame(swc.seat->pointer);

	return true;
}

EXPORT bool
swc_initialize(struct wl_display *display, struct wl_event_loop *event_loop,
               const struct swc_manager *manager)
{
	swc.display = display;
	swc.event_loop =
	    event_loop ? event_loop : wl_display_get_event_loop(display);
	swc.manager = manager;
	const char *default_seat = "seat0";
	wl_signal_init(&swc.event_signal);

	if (!launch_initialize()) {
		ERROR("Could not connect to swc-launch\n");
		goto error0;
	}

	if (!
#ifdef ENABLE_DRM
	    drm_initialize()
#else
	    fb_initialize()
#endif
	) {
		ERROR("Could not initialize video backend\n");
		goto error1;
	}

	swc.shm = shm_create(display);
	if (!swc.shm) {
		ERROR("Could not initialize SHM\n");
		goto error2;
	}

	if (!bindings_initialize()) {
		ERROR("Could not initialize bindings\n");
		goto error3;
	}

	swc.subcompositor = subcompositor_create(display);
	if (!swc.subcompositor) {
		ERROR("Could not initialize subcompositor\n");
		goto error4;
	}

	if (!screens_initialize()) {
		ERROR("Could not initialize screens\n");
		goto error5;
	}

	if (!compositor_initialize()) {
		ERROR("Could not initialize compositor\n");
		goto error6;
	}

	swc.data_device_manager = data_device_manager_create(display);
	if (!swc.data_device_manager) {
		ERROR("Could not initialize data device manager\n");
		goto error7;
	}

	swc.seat = seat_create(display, default_seat);
	if (!swc.seat) {
		ERROR("Could not initialize seat\n");
		goto error8;
	}

	swc.shell = shell_create(display);
	if (!swc.shell) {
		ERROR("Could not initialize shell\n");
		goto error9;
	}

	swc.xdg_shell = xdg_shell_create(display);
	if (!swc.xdg_shell) {
		ERROR("Could not initialize XDG shell\n");
		goto error10;
	}

	swc.xdg_decoration_manager = xdg_decoration_manager_create(display);
	if (!swc.xdg_decoration_manager) {
		ERROR("Could not initialize XDG decoration manager\n");
		goto error11;
	}

	swc.kde_decoration_manager = kde_decoration_manager_create(display);
	if (!swc.kde_decoration_manager) {
		ERROR("Could not initialize KDE decoration manager\n");
		goto error12;
	}

	swc.layer_shell = layer_shell_create(display);
	if (!swc.layer_shell) {
		ERROR("Could not initialize layer shell\n");
		goto error13;
	}

	swc.panel_manager = panel_manager_create(display);
	if (!swc.panel_manager) {
		ERROR("Could not initialize panel manager\n");
		goto error14;
	}

	swc.snap_manager = snap_manager_create(display);
	if (!swc.snap_manager) {
		ERROR("Could not initialize snap manager\n");
		goto error15;
	}

#ifdef ENABLE_XWAYLAND
	if (!xserver_initialize()) {
		ERROR("Could not initialize xwayland\n");
		goto error16;
	}
#endif

	swc.select_manager = select_manager_create(display);
	if (!swc.select_manager) {
		ERROR("Could not initialize select manager\n");
		goto error17;
	}

	swc.xdg_output_manager = xdg_output_manager_create(display);
	if (!swc.xdg_output_manager) {
		ERROR("Could not initialize XDG output manager\n");
		goto error17;
	}

	setup_compositor();

#ifdef ENABLE_DARWIN
	/* IOKit mediates display and input access, so Darwin has no launcher
	 * process to deliver the initial VT activation event. */
	swc_activate();
#endif

	return true;

error17:
	wl_global_destroy(swc.select_manager);
#ifdef ENABLE_XWAYLAND
error16:
#endif
	wl_global_destroy(swc.snap_manager);
error15:
	wl_global_destroy(swc.panel_manager);
error14:
	wl_global_destroy(swc.layer_shell);
error13:
	wl_global_destroy(swc.kde_decoration_manager);
error12:
	wl_global_destroy(swc.xdg_decoration_manager);
error11:
	wl_global_destroy(swc.xdg_shell);
error10:
	wl_global_destroy(swc.shell);
error9:
	seat_destroy(swc.seat);
error8:
	wl_global_destroy(swc.data_device_manager);
error7:
	compositor_finalize();
error6:
	screens_finalize();
error5:
	wl_global_destroy(swc.subcompositor);
error4:
	bindings_finalize();
error3:
	shm_destroy(swc.shm);
error2:
#ifdef ENABLE_DRM
	drm_finalize();
#else
	fb_finalize();
#endif
error1:
	launch_finalize();
error0:
	return false;
}

EXPORT void
swc_finalize(void)
{
#ifdef ENABLE_XWAYLAND
	xserver_finalize();
#endif
	wl_global_destroy(swc.xdg_output_manager);
	wl_global_destroy(swc.snap_manager);
	wl_global_destroy(swc.select_manager);
	wl_global_destroy(swc.panel_manager);
	wl_global_destroy(swc.layer_shell);
	wl_global_destroy(swc.xdg_decoration_manager);
	wl_global_destroy(swc.xdg_shell);
	wl_global_destroy(swc.shell);
	seat_destroy(swc.seat);
	wl_global_destroy(swc.data_device_manager);
	compositor_finalize();
	screens_finalize();
	bindings_finalize();
	shm_destroy(swc.shm);
#ifdef ENABLE_DRM
	drm_finalize();
#else
	fb_finalize();
#endif
	launch_finalize();
}
