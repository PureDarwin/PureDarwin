/* swc: libswc/launch.c
 *
 * Copyright (c) 2013, 2014 Michael Forney
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

#include "launch.h"
#include "event.h"
#include "internal.h"
#include "launch/protocol.h"
#include "util.h"

#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <wayland-server.h>

#ifdef ENABLE_DARWIN

bool
launch_initialize(void)
{
	return true;
}

void
launch_finalize(void)
{
}

int
launch_open_device(const char *path, int flags)
{
	return open(path, flags);
}

bool
launch_activate_vt(unsigned vt)
{
	(void)vt;
	return false;
}

#else

static struct {
	int socket;
	struct wl_event_source *source;
	uint32_t next_serial;
} launch;

static bool
handle_event(struct swc_launch_event *event)
{
	switch (event->type) {
	case SWC_LAUNCH_EVENT_ACTIVATE:
		swc_activate();
		break;
	case SWC_LAUNCH_EVENT_DEACTIVATE:
		swc_deactivate();
		break;
	default:
		return false;
	}

	return true;
}

static int
handle_data(int fd, uint32_t mask, void *data)
{
	struct swc_launch_event event;
	struct iovec iov[1] = {
	    {.iov_base = &event, .iov_len = sizeof(event)},
	};

	if (receive_fd(fd, NULL, iov, 1) != -1) {
		handle_event(&event);
	}
	return 1;
}

bool
launch_initialize(void)
{
	char *socket_string, *end;

	if (!(socket_string = getenv(SWC_LAUNCH_SOCKET_ENV))) {
		return false;
	}

	launch.socket = strtol(socket_string, &end, 10);
	if (*end != '\0') {
		return false;
	}

	unsetenv(SWC_LAUNCH_SOCKET_ENV);
	if (fcntl(launch.socket, F_SETFD, FD_CLOEXEC) < 0) {
		return false;
	}

	launch.source = wl_event_loop_add_fd(swc.event_loop, launch.socket,
	                                     WL_EVENT_READABLE, &handle_data, NULL);
	if (!launch.source) {
		return false;
	}

	return true;
}

void
launch_finalize(void)
{
	wl_event_source_remove(launch.source);
	close(launch.socket);
}

static bool
send_request(struct swc_launch_request *request, const void *data, size_t size,
             struct swc_launch_event *event, int out_fd, int *in_fd)
{
	struct iovec request_iov[2] = {
	    {.iov_base = request, .iov_len = sizeof(*request)},
	    {.iov_base = (void *)data, .iov_len = size},
	};
	struct iovec response_iov[1] = {
	    {.iov_base = event, .iov_len = sizeof(*event)},
	};

	request->serial = ++launch.next_serial;

	if (send_fd(launch.socket, out_fd, request_iov, 1 + (size > 0)) == -1) {
		return false;
	}

	while (receive_fd(launch.socket, in_fd, response_iov, 1) != -1) {
		if (event->type == SWC_LAUNCH_EVENT_RESPONSE &&
		    event->serial == request->serial) {
			return true;
		}
		handle_event(event);
	}

	return false;
}

int
launch_open_device(const char *path, int flags)
{
	struct swc_launch_request request;
	struct swc_launch_event response;
	int fd;

	request.type = SWC_LAUNCH_REQUEST_OPEN_DEVICE;
	request.flags = flags;

	if (!send_request(&request, path, strlen(path) + 1, &response, -1, &fd)) {
		return -1;
	}

	return fd;
}

bool
launch_activate_vt(unsigned vt)
{
	struct swc_launch_request request;
	struct swc_launch_event response;

	request.type = SWC_LAUNCH_REQUEST_ACTIVATE_VT;
	request.vt = vt;

	if (!send_request(&request, NULL, 0, &response, -1, NULL)) {
		return false;
	}

	return response.success;
}

#endif
