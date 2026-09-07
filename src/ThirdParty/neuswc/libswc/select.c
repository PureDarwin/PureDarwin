#include <wayland-server.h>

#include "select.h"
#include "swc_select-server-protocol.h"

#include "internal.h"
#include "compositor.h"
#include "pointer.h"
#include "seat.h"
#include "util.h"

static struct wl_list select_resources;
static int32_t start_x, start_y;
static struct pointer_handler select_pointer_handler;

enum select_state { STATE_WAIT, STATE_DRAG };
static enum select_state select_state;

static bool
handle_motion(struct pointer_handler *h, uint32_t time, wl_fixed_t fx,
              wl_fixed_t fy)
{
	int32_t x = wl_fixed_to_int(fx);
	int32_t y = wl_fixed_to_int(fy);
	struct wl_resource *resource;

	if (select_state != STATE_DRAG) {
		return false;
	}

	/* TODO: customizable color, maybeee, idk idc */
	swc_overlay_set_box(start_x, start_y, x, y, 0xffffffff, 2);

	wl_resource_for_each(resource, &select_resources)
	    swc_select_send_update(resource, start_x, start_y, x, y);

	return true;
}

static bool
handle_button(struct pointer_handler *h, uint32_t time, struct button *button,
              uint32_t state)
{
	int32_t x = wl_fixed_to_int(swc.seat->pointer->x);
	int32_t y = wl_fixed_to_int(swc.seat->pointer->y);
	struct wl_resource *resource;

	if (state == WL_POINTER_BUTTON_STATE_PRESSED &&
	    select_state == STATE_WAIT) {
		start_x = x;
		start_y = y;
		select_state = STATE_DRAG;
		return true;
	}

	if (state == WL_POINTER_BUTTON_STATE_RELEASED &&
	    select_state == STATE_DRAG) {
		swc_overlay_clear();
		wl_list_remove(&select_pointer_handler.link);
		select_state = STATE_WAIT;
		swc_set_cursor(SWC_CURSOR_DEFAULT);

		wl_resource_for_each(resource, &select_resources)
		    swc_select_send_done(resource, start_x, start_y, x, y);

		return true;
	}

	return false;
}

static void
handle_grab(struct wl_client *client, struct wl_resource *resource)
{
	select_state = STATE_WAIT;
	select_pointer_handler.motion = handle_motion;
	select_pointer_handler.button = handle_button;
	wl_list_insert(&swc.seat->pointer->handlers, &select_pointer_handler.link);
	swc_set_cursor(SWC_CURSOR_CROSS);
}

static const struct swc_select_interface select_impl = {
    .grab = handle_grab,
};

static void
bind_select(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource;

	resource = wl_resource_create(client, &swc_select_interface, 1, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &select_impl, NULL,
	                               remove_resource);
	wl_list_insert(&select_resources, wl_resource_get_link(resource));
}

struct wl_global *
select_manager_create(struct wl_display *display)
{
	wl_list_init(&select_resources);
	select_pointer_handler.motion = handle_motion;
	select_pointer_handler.button = handle_button;
	return wl_global_create(display, &swc_select_interface, 1, NULL,
	                        &bind_select);
}
