/* swc: launch/protocol.h
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

#ifndef SWC_LAUNCH_PROTOCOL_H
#define SWC_LAUNCH_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define SWC_LAUNCH_SOCKET_ENV "SWC_LAUNCH_SOCKET"
#define SWC_LAUNCH_TTY_ENV "SWC_LAUNCH_TTY"

struct iovec;

struct swc_launch_request {
	enum {
		SWC_LAUNCH_REQUEST_OPEN_DEVICE,
		SWC_LAUNCH_REQUEST_ACTIVATE_VT,
	} type;

	uint32_t serial;

	union {
		struct /* OPEN_DEVICE */ {
			int flags;
		};
		struct /* ACTIVATE_VT */ {
			unsigned vt;
		};
	};
};

struct swc_launch_event {
	enum {
		SWC_LAUNCH_EVENT_RESPONSE,
		SWC_LAUNCH_EVENT_ACTIVATE,
		SWC_LAUNCH_EVENT_DEACTIVATE,
	} type;

	union {
		struct /* RESPONSE */ {
			uint32_t serial;
			bool success;
		};
	};
};

ssize_t
send_fd(int socket, int fd, struct iovec *iov, int iovlen);
ssize_t
receive_fd(int socket, int *fd, struct iovec *iov, int iovlen);

#endif
