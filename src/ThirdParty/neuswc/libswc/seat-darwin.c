/* swc: libswc/seat-darwin.c
 *
 * Copyright (c) 2026 neuswc contributors
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

#include "compositor.h"
#include "data_device.h"
#include "event.h"
#include "internal.h"
#include "keyboard.h"
#include "pointer.h"
#include "seat.h"
#include "surface.h"
#include "util.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDElement.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDValue.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/resource.h>
#include <mach/mach.h>
#include <string.h>
#include <unistd.h>
#include <mach/mach_time.h>
#include <wayland-server.h>

enum {
	HID_PAGE_GENERIC_DESKTOP = 0x01,
	HID_PAGE_KEYBOARD = 0x07,
	HID_PAGE_BUTTON = 0x09,
	HID_USAGE_X = 0x30,
	HID_USAGE_Y = 0x31,
	HID_USAGE_WHEEL = 0x38,
};

struct seat {
	struct swc_seat base;
	char *name;
	uint32_t capabilities;
	bool ignore;

	IOHIDManagerRef manager;
	struct wl_event_source *run_loop_source;
#ifdef __PUREDARWIN__
	int keyboard_fd;
	int mouse_fd;
	uint8_t mouse_buttons;
#endif

	struct wl_listener swc_listener;
	struct wl_listener keyboard_focus_listener;
	struct pointer pointer;
	struct wl_listener data_device_listener;

	struct wl_global *global;
	struct wl_list resources;

	int layout_token;
	struct wl_event_source *layout_source;
	int layout;
};

#ifdef __PUREDARWIN__
struct pd_hid_keyboard_event {
	uint32_t sequence;
	uint8_t usage;
	uint8_t down;
	uint8_t reserved[2];
};

struct pd_hid_mouse_event {
	uint32_t sequence;
	uint8_t mouse_index;
	uint8_t buttons;
	int8_t dx;
	int8_t dy;
	int8_t wheel;
	uint8_t reserved[3];
};
#endif

/* USB HID keyboard usages translated to Linux input keycodes, as expected by
 * libxkbcommon. Unlisted usages are not keyboard keys. */
static const uint16_t hid_to_evdev[256] = {
	[0x04] = 30, [0x05] = 48, [0x06] = 46, [0x07] = 32,
	[0x08] = 18, [0x09] = 33, [0x0a] = 34, [0x0b] = 35,
	[0x0c] = 23, [0x0d] = 36, [0x0e] = 37, [0x0f] = 38,
	[0x10] = 50, [0x11] = 49, [0x12] = 24, [0x13] = 25,
	[0x14] = 16, [0x15] = 19, [0x16] = 31, [0x17] = 20,
	[0x18] = 22, [0x19] = 47, [0x1a] = 17, [0x1b] = 45,
	[0x1c] = 21, [0x1d] = 44,
	[0x1e] = 2, [0x1f] = 3, [0x20] = 4, [0x21] = 5,
	[0x22] = 6, [0x23] = 7, [0x24] = 8, [0x25] = 9,
	[0x26] = 10, [0x27] = 11,
	[0x28] = 28, [0x29] = 1, [0x2a] = 14, [0x2b] = 15,
	[0x2c] = 57, [0x2d] = 12, [0x2e] = 13, [0x2f] = 26,
	[0x30] = 27, [0x31] = 43, [0x33] = 39, [0x34] = 40,
	[0x35] = 41, [0x36] = 51, [0x37] = 52, [0x38] = 53,
	[0x39] = 58,
	[0x3a] = 59, [0x3b] = 60, [0x3c] = 61, [0x3d] = 62,
	[0x3e] = 63, [0x3f] = 64, [0x40] = 65, [0x41] = 66,
	[0x42] = 67, [0x43] = 68, [0x44] = 87, [0x45] = 88,
	[0x49] = 110, [0x4a] = 102, [0x4b] = 104, [0x4c] = 111,
	[0x4d] = 107, [0x4e] = 109, [0x4f] = 106, [0x50] = 105,
	[0x51] = 108, [0x52] = 103,
	[0x53] = 69, [0x54] = 98, [0x55] = 55, [0x56] = 74,
	[0x57] = 78, [0x58] = 96, [0x59] = 79, [0x5a] = 80,
	[0x5b] = 81, [0x5c] = 75, [0x5d] = 76, [0x5e] = 77,
	[0x5f] = 71, [0x60] = 72, [0x61] = 73, [0x62] = 82,
	[0x63] = 83, [0x65] = 127,
	[0xe0] = 29, [0xe1] = 42, [0xe2] = 56, [0xe3] = 125,
	[0xe4] = 97, [0xe5] = 54, [0xe6] = 100, [0xe7] = 126,
};

static void
handle_keyboard_focus_event(struct wl_listener *listener, void *data)
{
	struct seat *seat =
	    wl_container_of(listener, seat, keyboard_focus_listener);
	struct event *ev = data;
	struct input_focus_event_data *event_data = ev->data;

	if (ev->type == INPUT_FOCUS_EVENT_CHANGED && event_data->new) {
		struct wl_client *client =
		    wl_resource_get_client(event_data->new->surface->resource);
		data_device_offer_selection(seat->base.data_device, client);
	}
}

static void
handle_data_device_event(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, data_device_listener);
	struct event *ev = data;

	if (ev->type == DATA_DEVICE_EVENT_SELECTION_CHANGED &&
	    seat->base.keyboard->focus.client) {
		data_device_offer_selection(seat->base.data_device,
		                            seat->base.keyboard->focus.client);
	}
}

static void
handle_swc_event(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swc_listener);
	struct event *ev = data;

	if (ev->type == SWC_EVENT_DEACTIVATED) {
		seat->ignore = true;
		keyboard_reset(seat->base.keyboard);
	} else if (ev->type == SWC_EVENT_ACTIVATED) {
		seat->ignore = false;
	}
}

static void
get_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	struct seat *seat = wl_resource_get_user_data(resource);
	pointer_bind(&seat->pointer, client, wl_resource_get_version(resource), id);
}

static void
get_keyboard(struct wl_client *client, struct wl_resource *resource,
             uint32_t id)
{
	struct seat *seat = wl_resource_get_user_data(resource);
	keyboard_bind(seat->base.keyboard, client,
	              wl_resource_get_version(resource), id);
}

static void
get_touch(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	(void)client;
	(void)resource;
	(void)id;
}

static const struct wl_seat_interface seat_impl = {
	.get_pointer = get_pointer,
	.get_keyboard = get_keyboard,
	.get_touch = get_touch,
	.release = destroy_resource,
};

static void
bind_seat(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct seat *seat = data;
	struct wl_resource *resource;

	if (version > 5) {
		version = 5;
	}
	resource = wl_resource_create(client, &wl_seat_interface, version, id);
	wl_resource_set_implementation(resource, &seat_impl, seat,
	                               &remove_resource);
	wl_list_insert(&seat->resources, wl_resource_get_link(resource));
	if (version >= 2) {
		wl_seat_send_name(resource, seat->name);
	}
	wl_seat_send_capabilities(resource, seat->capabilities);
}

/* libnotify ships in libSystem without its header in this SDK */
extern uint32_t notify_register_check(const char *name, int *out_token);
extern uint32_t notify_set_state(int token, uint64_t state);

/* Every client reads the system idle time from this notify state, in
 * nanoseconds of mach_absolute_time, the moment of the last HID input. */
static void
publish_last_input(void)
{
	static int token = -1;
	static uint64_t published;
	static mach_timebase_info_data_t timebase;
	uint64_t now;

	if (timebase.denom == 0) {
		mach_timebase_info(&timebase);
	}
	now = mach_absolute_time() * timebase.numer / timebase.denom;
	if (now - published < 1000000000ULL) {
		return;
	}
	published = now;
	if (token < 0 &&
	    notify_register_check("org.puredarwin.hid.lastinput", &token) != 0) {
		return;
	}
	notify_set_state(token, now);
}

extern uint32_t notify_register_file_descriptor(const char *name, int *notify_fd,
                                                int flags, int *out_token);
extern uint32_t notify_get_state(int token, uint64_t *state);
extern uint32_t notify_cancel(int token);

#define LAYOUT_NOTIFY_NAME "com.apple.Carbon.TISNotifySelectedKeyboardInputSourceChanged"

// Carbon's layouts in its own order, with the xkb layout that types the same characters
static const struct {
	const char *source_id, *layout, *variant;
} layouts[] = {
	{"com.apple.keylayout.US", "us", "basic"},
	{"com.apple.keylayout.German", "de", "mac"},
	{"com.apple.keylayout.French", "fr", "mac"},
};

// the layout Carbon last selected, from the notify state or else the saved preference
static int
selected_layout(struct seat *seat)
{
	uint64_t state = 0;
	CFStringRef id;
	int found = -1;

	if (seat->layout_token >= 0 &&
	    notify_get_state(seat->layout_token, &state) == 0 &&
	    state >= 1 && state <= sizeof(layouts) / sizeof(layouts[0]))
		return (int)state - 1;

	CFPreferencesAppSynchronize(CFSTR("com.apple.HIToolbox"));
	id = CFPreferencesCopyAppValue(CFSTR("AppleCurrentKeyboardLayoutInputSourceID"),
	                               CFSTR("com.apple.HIToolbox"));
	if (id && CFGetTypeID(id) == CFStringGetTypeID()) {
		char buffer[128];

		if (CFStringGetCString(id, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
			for (size_t i = 0; i < sizeof(layouts) / sizeof(layouts[0]); i++) {
				if (!strcmp(buffer, layouts[i].source_id))
					found = (int)i;
			}
		}
	}
	if (id)
		CFRelease(id);
	return found;
}

static void
apply_layout(struct seat *seat)
{
	int layout = selected_layout(seat);
	struct xkb_rule_names names = {
		.rules = "evdev",
		.model = "pc105",
	};

	if (layout < 0 || layout == seat->layout)
		return;
	names.layout = layouts[layout].layout;
	names.variant = layouts[layout].variant;
	if (keyboard_set_keymap(seat->base.keyboard, &names)) {
		seat->layout = layout;
		DEBUG("Keyboard layout now %s(%s)\n", names.layout, names.variant);
	}
}

static int
handle_layout_notify(int fd, uint32_t mask, void *data)
{
	struct seat *seat = data;
	int token;

	(void)mask;
	// each post leaves one token on the descriptor
	while (read(fd, &token, sizeof(token)) == sizeof(token))
		;
	apply_layout(seat);
	return 0;
}

static void
watch_layout(struct seat *seat)
{
	int fd = -1, check;

	seat->layout = 0;
	seat->layout_token = -1;
	if (notify_register_check(LAYOUT_NOTIFY_NAME, &check) == 0)
		seat->layout_token = check;
	if (notify_register_file_descriptor(LAYOUT_NOTIFY_NAME, &fd, 0, &check) == 0) {
		fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
		seat->layout_source = wl_event_loop_add_fd(swc.event_loop, fd, WL_EVENT_READABLE,
		                                           handle_layout_notify, seat);
	}
	apply_layout(seat);
}

static void
handle_hid_value(void *context, IOReturn result, void *sender,
                 IOHIDValueRef value)
{
	struct seat *seat = context;
	IOHIDElementRef element;
	uint32_t page, usage, time;
	CFIndex integer;

	(void)sender;
	if (result != kIOReturnSuccess || seat->ignore) {
		return;
	}
	element = IOHIDValueGetElement(value);
	page = IOHIDElementGetUsagePage(element);
	usage = IOHIDElementGetUsage(element);
	integer = IOHIDValueGetIntegerValue(value);
	time = get_time();
	publish_last_input();

	if (page == HID_PAGE_KEYBOARD && usage < 256 && hid_to_evdev[usage]) {
		keyboard_handle_key(seat->base.keyboard, time, hid_to_evdev[usage],
		                    integer ? WL_KEYBOARD_KEY_STATE_PRESSED
		                            : WL_KEYBOARD_KEY_STATE_RELEASED);
		return;
	}
	if (page == HID_PAGE_BUTTON && usage > 0 && usage <= 16) {
		uint32_t button = 0x110 + usage - 1;
		if (usage == 2) {
			button = 0x111;
		} else if (usage == 3) {
			button = 0x112;
		}
		pointer_handle_button(seat->base.pointer, time, button,
		                      integer ? WL_POINTER_BUTTON_STATE_PRESSED
		                              : WL_POINTER_BUTTON_STATE_RELEASED);
		pointer_handle_frame(seat->base.pointer);
		return;
	}
	if (page != HID_PAGE_GENERIC_DESKTOP) {
		return;
	}
	if (usage == HID_USAGE_X && IOHIDElementIsRelative(element)) {
		pointer_handle_relative_motion(seat->base.pointer, time,
		                               wl_fixed_from_int(integer), 0);
	} else if (usage == HID_USAGE_Y && IOHIDElementIsRelative(element)) {
		pointer_handle_relative_motion(seat->base.pointer, time, 0,
		                               wl_fixed_from_int(integer));
	} else if (usage == HID_USAGE_WHEEL) {
		pointer_handle_axis(seat->base.pointer, time,
		                    WL_POINTER_AXIS_VERTICAL_SCROLL,
		                    WL_POINTER_AXIS_SOURCE_WHEEL,
		                    wl_fixed_from_int(-integer * 10), -integer * 120);
	}
	pointer_handle_frame(seat->base.pointer);
}

// ARGON_GFX_STATS: where the compositor's CPU goes, input polling against everything else, every 5 s
static struct {
	int on;
	unsigned long polls;
	unsigned long long poll_ns, cf_ns;
	struct timespec since;
	double cpu_since;
} idle_stats = { -1 };

static double
process_cpu(void)
{
	struct rusage usage;

	getrusage(RUSAGE_SELF, &usage);
	return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6 + usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
}

static unsigned long long
elapsed_ns(const struct timespec *a, const struct timespec *b)
{
	return (unsigned long long)(b->tv_sec - a->tv_sec) * 1000000000ull + (b->tv_nsec - a->tv_nsec);
}

static void
idle_stats_report(void)
{
	struct timespec now;
	double cpu;

	clock_gettime(CLOCK_MONOTONIC, &now);
	if (now.tv_sec - idle_stats.since.tv_sec < 5)
		return;
	cpu = process_cpu();
	fprintf(stderr, "ARGON-SEAT %lu polls, poll %.1f ms (CFRunLoop %.1f ms), process cpu %.1f ms in %.1f s\n",
	        idle_stats.polls, idle_stats.poll_ns / 1e6, idle_stats.cf_ns / 1e6, (cpu - idle_stats.cpu_since) * 1000,
	        elapsed_ns(&idle_stats.since, &now) / 1e9);

	// each thread's total cpu, to tell the event loop from helper threads
	thread_act_array_t threads;
	mach_msg_type_number_t count;

	if (task_threads(mach_task_self(), &threads, &count) == KERN_SUCCESS) {
		for (mach_msg_type_number_t i = 0; i < count; i++) {
			thread_basic_info_data_t info;
			mach_msg_type_number_t size = THREAD_BASIC_INFO_COUNT;

			if (thread_info(threads[i], THREAD_BASIC_INFO, (thread_info_t)&info, &size) == KERN_SUCCESS)
				fprintf(stderr, "ARGON-THREAD %u%s user %d.%06d sys %d.%06d\n", i,
				        threads[i] == mach_thread_self() ? " (main)" : "",
				        info.user_time.seconds, info.user_time.microseconds,
				        info.system_time.seconds, info.system_time.microseconds);
			mach_port_deallocate(mach_task_self(), threads[i]);
		}
		vm_deallocate(mach_task_self(), (vm_address_t)threads, count * sizeof(*threads));
	}
	idle_stats.polls = 0;
	idle_stats.poll_ns = idle_stats.cf_ns = 0;
	idle_stats.since = now;
	idle_stats.cpu_since = cpu;
}

static int dispatch_run_loop_body(struct seat *seat);

static int
dispatch_run_loop(void *data)
{
	struct timespec start, end;
	int result;

	if (idle_stats.on < 0) {
		idle_stats.on = getenv("ARGON_GFX_STATS") != NULL;
		clock_gettime(CLOCK_MONOTONIC, &idle_stats.since);
		idle_stats.cpu_since = process_cpu();
	}
	clock_gettime(CLOCK_MONOTONIC, &start);
	result = dispatch_run_loop_body(data);
	if (idle_stats.on) {
		clock_gettime(CLOCK_MONOTONIC, &end);
		idle_stats.polls++;
		idle_stats.poll_ns += elapsed_ns(&start, &end);
		idle_stats_report();
	}
	return result;
}

static int
dispatch_run_loop_body(struct seat *seat)
{
#ifdef __PUREDARWIN__
	struct pd_hid_keyboard_event keyboard_event;
	struct pd_hid_mouse_event mouse_event;

	if (seat->keyboard_fd < 0) {
		seat->keyboard_fd = open("/dev/usb_hid_kbd", O_RDONLY | O_NONBLOCK);
	}
	if (seat->mouse_fd < 0) {
		seat->mouse_fd = open("/dev/usb_hid_mouse", O_RDONLY | O_NONBLOCK);
	}
	while (seat->keyboard_fd >= 0 &&
	       read(seat->keyboard_fd, &keyboard_event, sizeof(keyboard_event)) ==
	           sizeof(keyboard_event)) {
		uint16_t key = hid_to_evdev[keyboard_event.usage];

		if (key) {
			keyboard_handle_key(seat->base.keyboard, get_time(), key,
			                    keyboard_event.down
			                        ? WL_KEYBOARD_KEY_STATE_PRESSED
			                        : WL_KEYBOARD_KEY_STATE_RELEASED);
		}
	}
	while (seat->mouse_fd >= 0 &&
	       read(seat->mouse_fd, &mouse_event, sizeof(mouse_event)) ==
	           sizeof(mouse_event)) {
		uint32_t time = get_time();
		uint8_t changed = mouse_event.buttons ^ seat->mouse_buttons;
		unsigned bit;

		if (mouse_event.dx || mouse_event.dy) {
			pointer_handle_relative_motion(seat->base.pointer, time,
			                               wl_fixed_from_int(mouse_event.dx),
			                               wl_fixed_from_int(mouse_event.dy));
		}
		for (bit = 0; bit < 3; ++bit) {
			if (changed & (1u << bit)) {
				pointer_handle_button(
				    seat->base.pointer, time, 0x110 + bit,
				    mouse_event.buttons & (1u << bit)
				        ? WL_POINTER_BUTTON_STATE_PRESSED
				        : WL_POINTER_BUTTON_STATE_RELEASED);
			}
		}
		seat->mouse_buttons = mouse_event.buttons;
		if (mouse_event.wheel) {
			pointer_handle_axis(seat->base.pointer, time,
			                    WL_POINTER_AXIS_VERTICAL_SCROLL,
			                    WL_POINTER_AXIS_SOURCE_WHEEL,
			                    wl_fixed_from_int(-mouse_event.wheel * 10),
			                    -mouse_event.wheel * 120);
		}
		pointer_handle_frame(seat->base.pointer);
	}
#else
	/* A zero interval returns before PureDarwin's run loop services the HID
	 * Mach port. Keep the wait bounded so Wayland remains the outer loop. */
	struct timespec cf_start, cf_end;

	clock_gettime(CLOCK_MONOTONIC, &cf_start);
	(void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, true);
	clock_gettime(CLOCK_MONOTONIC, &cf_end);
	idle_stats.cf_ns += elapsed_ns(&cf_start, &cf_end);
#endif
	wl_event_source_timer_update(seat->run_loop_source, 8);
	return 0;
}

static bool
initialize_hid(struct seat *seat)
{
#ifdef __PUREDARWIN__
	seat->keyboard_fd = -1;
	seat->mouse_fd = -1;
	return true;
#else
	seat->manager = IOHIDManagerCreate(kCFAllocatorDefault,
	                                   kIOHIDManagerOptionNone);
	if (!seat->manager) {
		return false;
	}
	IOHIDManagerSetDeviceMatching(seat->manager, NULL);
	IOHIDManagerRegisterInputValueCallback(seat->manager, handle_hid_value,
	                                       seat);
	IOHIDManagerScheduleWithRunLoop(seat->manager, CFRunLoopGetCurrent(),
	                                kCFRunLoopDefaultMode);
	if (IOHIDManagerOpen(seat->manager, kIOHIDOptionsTypeNone) !=
	    kIOReturnSuccess) {
		IOHIDManagerUnscheduleFromRunLoop(seat->manager, CFRunLoopGetCurrent(),
		                                    kCFRunLoopDefaultMode);
		CFRelease(seat->manager);
		seat->manager = NULL;
		return false;
	}
	return true;
#endif
}

struct swc_seat *
seat_create(struct wl_display *display, const char *seat_name)
{
	/* Keys reach xkb as evdev codes plus 8 (see XKB_KEY), so the keymap has
	 * to be built with the evdev rules. The base rules give the legacy
	 * xfree86 keycodes, where only the codes that happen to coincide work:
	 * letters and shift do, Super does not. */
	struct xkb_rule_names names = {
		.rules = "evdev",
		.model = "pc105",
		.layout = "us",
		.variant = "basic",
	};
	struct seat *seat = calloc(1, sizeof(*seat));

	if (!seat) {
		return NULL;
	}
	seat->name = strdup(seat_name);
	if (!seat->name) {
		goto error0;
	}
	seat->global =
	    wl_global_create(display, &wl_seat_interface, 5, seat, bind_seat);
	if (!seat->global) {
		goto error1;
	}
	seat->capabilities =
	    WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER;
	wl_list_init(&seat->resources);

	seat->swc_listener.notify = handle_swc_event;
	wl_signal_add(&swc.event_signal, &seat->swc_listener);
	seat->base.data_device = data_device_create();
	if (!seat->base.data_device) {
		goto error2;
	}
	seat->data_device_listener.notify = handle_data_device_event;
	wl_signal_add(&seat->base.data_device->event_signal,
	              &seat->data_device_listener);
	seat->base.keyboard = keyboard_create(&names);
	if (!seat->base.keyboard) {
		goto error3;
	}
	seat->keyboard_focus_listener.notify = handle_keyboard_focus_event;
	wl_signal_add(&seat->base.keyboard->focus.event_signal,
	              &seat->keyboard_focus_listener);
	if (!pointer_initialize(&seat->pointer)) {
		goto error4;
	}
	seat->base.pointer = &seat->pointer;
	if (!initialize_hid(seat)) {
		ERROR("Could not initialize IOHIDManager\n");
		goto error5;
	}
	seat->run_loop_source =
	    wl_event_loop_add_timer(swc.event_loop, dispatch_run_loop, seat);
	if (!seat->run_loop_source) {
		goto error6;
	}
	/* A zero timeout disarms timers on Darwin. Run the initial poll directly;
	 * it schedules subsequent polls after the HID nodes have been checked. */
	dispatch_run_loop(seat);
	watch_layout(seat);
	return &seat->base;

error6:
#ifdef __PUREDARWIN__
	if (seat->keyboard_fd >= 0) {
		close(seat->keyboard_fd);
	}
	if (seat->mouse_fd >= 0) {
		close(seat->mouse_fd);
	}
#else
	IOHIDManagerClose(seat->manager, kIOHIDOptionsTypeNone);
	IOHIDManagerUnscheduleFromRunLoop(seat->manager, CFRunLoopGetCurrent(),
	                                    kCFRunLoopDefaultMode);
	CFRelease(seat->manager);
#endif
error5:
	pointer_finalize(&seat->pointer);
error4:
	keyboard_destroy(seat->base.keyboard);
error3:
	data_device_destroy(seat->base.data_device);
error2:
	wl_global_destroy(seat->global);
error1:
	free(seat->name);
error0:
	free(seat);
	return NULL;
}

void
seat_destroy(struct swc_seat *seat_base)
{
	struct seat *seat = wl_container_of(seat_base, seat, base);

	wl_event_source_remove(seat->run_loop_source);
	if (seat->layout_source)
		wl_event_source_remove(seat->layout_source);
	if (seat->layout_token >= 0)
		notify_cancel(seat->layout_token);
#ifdef __PUREDARWIN__
	if (seat->keyboard_fd >= 0) {
		close(seat->keyboard_fd);
	}
	if (seat->mouse_fd >= 0) {
		close(seat->mouse_fd);
	}
#else
	IOHIDManagerClose(seat->manager, kIOHIDOptionsTypeNone);
	IOHIDManagerUnscheduleFromRunLoop(seat->manager, CFRunLoopGetCurrent(),
	                                    kCFRunLoopDefaultMode);
	CFRelease(seat->manager);
#endif
	pointer_finalize(&seat->pointer);
	keyboard_destroy(seat->base.keyboard);
	data_device_destroy(seat->base.data_device);
	wl_global_destroy(seat->global);
	free(seat->name);
	free(seat);
}
