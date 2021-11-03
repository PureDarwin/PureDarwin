/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

#include "internal.h"
#if DISPATCH_EVENT_BACKEND_WINDOWS

static HANDLE hPort = NULL;
enum _dispatch_windows_port {
	DISPATCH_PORT_POKE = 0,
	DISPATCH_PORT_TIMER_CLOCK_WALL,
	DISPATCH_PORT_TIMER_CLOCK_UPTIME,
	DISPATCH_PORT_TIMER_CLOCK_MONOTONIC,
	DISPATCH_PORT_FILE_HANDLE,
	DISPATCH_PORT_PIPE_HANDLE_READ,
	DISPATCH_PORT_PIPE_HANDLE_WRITE,
	DISPATCH_PORT_SOCKET_READ,
	DISPATCH_PORT_SOCKET_WRITE,
};

enum _dispatch_muxnote_events {
	DISPATCH_MUXNOTE_EVENT_READ = 1 << 0,
	DISPATCH_MUXNOTE_EVENT_WRITE = 1 << 1,
};

#pragma mark dispatch_unote_t

typedef struct dispatch_muxnote_s {
	LIST_ENTRY(dispatch_muxnote_s) dmn_list;
	LIST_HEAD(, dispatch_unote_linkage_s) dmn_readers_head;
	LIST_HEAD(, dispatch_unote_linkage_s) dmn_writers_head;

	// This refcount solves a race condition that can happen with I/O completion
	// ports. When we enqueue packets with muxnote pointers associated with
	// them, it's possible that those packets might not be processed until after
	// the event has been unregistered. We increment this upon creating a
	// muxnote or posting to a completion port, and we decrement it upon
	// unregistering the event or processing a packet. When it hits zero, we
	// dispose the muxnote.
	os_atomic(uintptr_t) dmn_refcount;

	dispatch_unote_ident_t dmn_ident;
	int8_t dmn_filter;
	enum _dispatch_muxnote_handle_type {
		DISPATCH_MUXNOTE_HANDLE_TYPE_INVALID,
		DISPATCH_MUXNOTE_HANDLE_TYPE_FILE,
		DISPATCH_MUXNOTE_HANDLE_TYPE_PIPE,
		DISPATCH_MUXNOTE_HANDLE_TYPE_SOCKET,
	} dmn_handle_type;
	enum _dispatch_muxnote_events dmn_events;

	// For pipes, this event is used to synchronize the monitoring thread with
	// I/O completion port processing. For sockets, this is the event used with
	// WSAEventSelect().
	HANDLE dmn_event;

	// Pipe monitoring thread control
	HANDLE dmn_thread;
	os_atomic(bool) dmn_stop;

	// Socket events registered with WSAEventSelect()
	long dmn_network_events;

	// Threadpool wait handle for socket events
	PTP_WAIT dmn_threadpool_wait;
} *dispatch_muxnote_t;

static LIST_HEAD(dispatch_muxnote_bucket_s, dispatch_muxnote_s)
    _dispatch_sources[DSL_HASH_SIZE];

DISPATCH_ALWAYS_INLINE
static inline struct dispatch_muxnote_bucket_s *
_dispatch_unote_muxnote_bucket(uint32_t ident)
{
	return &_dispatch_sources[DSL_HASH(ident)];
}

DISPATCH_ALWAYS_INLINE
static inline dispatch_muxnote_t
_dispatch_unote_muxnote_find(struct dispatch_muxnote_bucket_s *dmb,
		dispatch_unote_ident_t ident, int8_t filter)
{
	dispatch_muxnote_t dmn;
	if (filter == EVFILT_WRITE) filter = EVFILT_READ;
	LIST_FOREACH(dmn, dmb, dmn_list) {
		if (dmn->dmn_ident == ident && dmn->dmn_filter == filter) {
			break;
		}
	}
	return dmn;
}

static dispatch_muxnote_t
_dispatch_muxnote_create(dispatch_unote_t du,
		enum _dispatch_muxnote_events events)
{
	dispatch_muxnote_t dmn;
	int8_t filter = du._du->du_filter;
	HANDLE handle = (HANDLE)du._du->du_ident;

	dmn = _dispatch_calloc(1, sizeof(*dmn));
	if (dmn == NULL) {
		DISPATCH_INTERNAL_CRASH(0, "_dispatch_calloc");
	}
	os_atomic_store(&dmn->dmn_refcount, 1, relaxed);
	dmn->dmn_ident = (dispatch_unote_ident_t)handle;
	dmn->dmn_filter = filter;
	dmn->dmn_events = events;
	LIST_INIT(&dmn->dmn_readers_head);
	LIST_INIT(&dmn->dmn_writers_head);

	switch (filter) {
	case EVFILT_SIGNAL:
		WIN_PORT_ERROR();
		free(dmn);
		return NULL;

	case EVFILT_WRITE:
	case EVFILT_READ:
		switch (GetFileType(handle)) {
		case FILE_TYPE_UNKNOWN:
			// ensure that an invalid handle was not passed
			(void)dispatch_assume(GetLastError() == NO_ERROR);
			DISPATCH_INTERNAL_CRASH(0, "unknown handle type");

		case FILE_TYPE_REMOTE:
			DISPATCH_INTERNAL_CRASH(0, "unused handle type");

		case FILE_TYPE_CHAR:
			// The specified file is a character file, typically a
			// LPT device or a console.
			WIN_PORT_ERROR();
			free(dmn);
			return NULL;

		case FILE_TYPE_DISK:
			// The specified file is a disk file
			dmn->dmn_handle_type = DISPATCH_MUXNOTE_HANDLE_TYPE_FILE;
			break;

		case FILE_TYPE_PIPE:
			// The specified file is a socket, a named pipe, or an
			// anonymous pipe.
			dmn->dmn_handle_type = _dispatch_handle_is_socket(handle)
					? DISPATCH_MUXNOTE_HANDLE_TYPE_SOCKET
					: DISPATCH_MUXNOTE_HANDLE_TYPE_PIPE;
			break;
		}

		break;

	default:
		DISPATCH_INTERNAL_CRASH(0, "unexpected filter");
	}


	return dmn;
}

static void
_dispatch_muxnote_stop(dispatch_muxnote_t dmn)
{
	if (dmn->dmn_thread) {
		// Keep trying to cancel ReadFile() until the thread exits
		os_atomic_store(&dmn->dmn_stop, true, relaxed);
		SetEvent(dmn->dmn_event);
		do {
			CancelIoEx((HANDLE)dmn->dmn_ident, /* lpOverlapped */ NULL);
		} while (WaitForSingleObject(dmn->dmn_thread, 1) == WAIT_TIMEOUT);
		CloseHandle(dmn->dmn_thread);
		dmn->dmn_thread = NULL;
	}
	if (dmn->dmn_threadpool_wait) {
		SetThreadpoolWait(dmn->dmn_threadpool_wait, NULL, NULL);
		WaitForThreadpoolWaitCallbacks(dmn->dmn_threadpool_wait,
				/* fCancelPendingCallbacks */ FALSE);
		CloseThreadpoolWait(dmn->dmn_threadpool_wait);
		dmn->dmn_threadpool_wait = NULL;
	}
	if (dmn->dmn_handle_type == DISPATCH_MUXNOTE_HANDLE_TYPE_SOCKET) {
		WSAEventSelect((SOCKET)dmn->dmn_ident, NULL, 0);
	}
}

static void
_dispatch_muxnote_dispose(dispatch_muxnote_t dmn)
{
	if (dmn->dmn_thread || dmn->dmn_threadpool_wait) {
		DISPATCH_INTERNAL_CRASH(0, "disposed a muxnote with an active thread");
	}
	if (dmn->dmn_event) {
		CloseHandle(dmn->dmn_event);
	}
	free(dmn);
}

static void
_dispatch_muxnote_retain(dispatch_muxnote_t dmn)
{
	uintptr_t refcount = os_atomic_inc(&dmn->dmn_refcount, relaxed);
	if (refcount == 0) {
		DISPATCH_INTERNAL_CRASH(0, "muxnote refcount overflow");
	}
	if (refcount == 1) {
		DISPATCH_INTERNAL_CRASH(0, "retained a disposing muxnote");
	}
}

static void
_dispatch_muxnote_release(dispatch_muxnote_t dmn)
{
	uintptr_t refcount = os_atomic_dec(&dmn->dmn_refcount, relaxed);
	if (refcount == 0) {
		_dispatch_muxnote_dispose(dmn);
	} else if (refcount == UINTPTR_MAX) {
		DISPATCH_INTERNAL_CRASH(0, "muxnote refcount underflow");
	}
}

static unsigned WINAPI
_dispatch_pipe_monitor_thread(void *context)
{
	dispatch_muxnote_t dmn = (dispatch_muxnote_t)context;
	HANDLE hPipe = (HANDLE)dmn->dmn_ident;
	do {
		char cBuffer[1];
		DWORD dwNumberOfBytesTransferred;
		OVERLAPPED ov = {0};
		BOOL bSuccess = ReadFile(hPipe, cBuffer, /* nNumberOfBytesToRead */ 0,
				&dwNumberOfBytesTransferred, &ov);
		DWORD dwBytesAvailable;
		DWORD dwError = GetLastError();
		if (!bSuccess && dwError == ERROR_IO_PENDING) {
			bSuccess = GetOverlappedResult(hPipe, &ov,
					&dwNumberOfBytesTransferred, /* bWait */ TRUE);
			dwError = GetLastError();
		}
		if (bSuccess) {
			bSuccess = PeekNamedPipe(hPipe, NULL, 0, NULL, &dwBytesAvailable,
					NULL);
			dwError = GetLastError();
		}
		if (bSuccess) {
			if (dwBytesAvailable == 0) {
				// This can happen with a zero-byte write. Try again.
				continue;
			}
		} else if (dwError == ERROR_NO_DATA) {
			// The pipe is nonblocking. Try again.
			Sleep(0);
			continue;
		} else {
			_dispatch_debug("pipe[0x%llx]: GetLastError() returned %lu",
					(long long)hPipe, dwError);
			if (dwError == ERROR_OPERATION_ABORTED) {
				continue;
			}
			os_atomic_store(&dmn->dmn_stop, true, relaxed);
			dwBytesAvailable = 0;
		}

		// Make sure the muxnote stays alive until the packet is dequeued
		_dispatch_muxnote_retain(dmn);

		// The lpOverlapped parameter does not actually need to point to an
		// OVERLAPPED struct. It's really just a pointer to pass back to
		// GetQueuedCompletionStatus().
		bSuccess = PostQueuedCompletionStatus(hPort,
				dwBytesAvailable, (ULONG_PTR)DISPATCH_PORT_PIPE_HANDLE_READ,
				(LPOVERLAPPED)dmn);
		if (!bSuccess) {
			DISPATCH_INTERNAL_CRASH(GetLastError(),
					"PostQueuedCompletionStatus");
		}

		// If data is written into the pipe and not read right away, ReadFile()
		// will keep returning immediately and we'll flood the completion port.
		// This event lets us synchronize with _dispatch_event_loop_drain() so
		// that we only post events when it's ready for them.
		WaitForSingleObject(dmn->dmn_event, INFINITE);
	} while (!os_atomic_load(&dmn->dmn_stop, relaxed));
	_dispatch_debug("pipe[0x%llx]: monitor exiting", (long long)hPipe);
	return 0;
}

static DWORD
_dispatch_pipe_write_availability(HANDLE hPipe)
{
	IO_STATUS_BLOCK iosb;
	FILE_PIPE_LOCAL_INFORMATION fpli;
	NTSTATUS status = _dispatch_NtQueryInformationFile(hPipe, &iosb, &fpli,
			sizeof(fpli), FilePipeLocalInformation);
	if (!NT_SUCCESS(status)) {
		return 1;
	}
	return fpli.WriteQuotaAvailable;
}

static VOID CALLBACK
_dispatch_socket_callback(PTP_CALLBACK_INSTANCE inst, void *context,
		PTP_WAIT pwa, TP_WAIT_RESULT res)
{
	dispatch_muxnote_t dmn = (dispatch_muxnote_t)context;
	SOCKET sock = (SOCKET)dmn->dmn_ident;
	WSANETWORKEVENTS events;
	if (WSAEnumNetworkEvents(sock, (WSAEVENT)dmn->dmn_event, &events) == 0) {
		long lNetworkEvents = events.lNetworkEvents;
		DWORD dwBytesAvailable = 1;
		if (lNetworkEvents & FD_CLOSE) {
			dwBytesAvailable = 0;
			// Post to all registered read and write handlers
			lNetworkEvents |= FD_READ | FD_WRITE;
		} else if (lNetworkEvents & FD_READ) {
			ioctlsocket(sock, FIONREAD, &dwBytesAvailable);
		}
		if (lNetworkEvents & FD_READ) {
			_dispatch_muxnote_retain(dmn);
			if (!PostQueuedCompletionStatus(hPort, dwBytesAvailable,
					(ULONG_PTR)DISPATCH_PORT_SOCKET_READ, (LPOVERLAPPED)dmn)) {
				DISPATCH_INTERNAL_CRASH(GetLastError(),
						"PostQueuedCompletionStatus");
			}
		}
		if (lNetworkEvents & FD_WRITE) {
			_dispatch_muxnote_retain(dmn);
			if (!PostQueuedCompletionStatus(hPort, dwBytesAvailable,
					(ULONG_PTR)DISPATCH_PORT_SOCKET_WRITE, (LPOVERLAPPED)dmn)) {
				DISPATCH_INTERNAL_CRASH(GetLastError(),
						"PostQueuedCompletionStatus");
			}
		}
	} else {
		_dispatch_debug("socket[0x%llx]: WSAEnumNetworkEvents() failed (%d)",
				(long long)sock, WSAGetLastError());
	}
	SetThreadpoolWait(pwa, dmn->dmn_event, /* pftTimeout */ NULL);
}

static BOOL
_dispatch_io_trigger(dispatch_muxnote_t dmn)
{
	BOOL bSuccess;
	long lNetworkEvents;

	switch (dmn->dmn_handle_type) {
	case DISPATCH_MUXNOTE_HANDLE_TYPE_INVALID:
		DISPATCH_INTERNAL_CRASH(0, "invalid handle");

	case DISPATCH_MUXNOTE_HANDLE_TYPE_FILE:
		_dispatch_muxnote_retain(dmn);
		bSuccess = PostQueuedCompletionStatus(hPort, 0,
			(ULONG_PTR)DISPATCH_PORT_FILE_HANDLE, (LPOVERLAPPED)dmn);
		if (bSuccess == FALSE) {
			DISPATCH_INTERNAL_CRASH(GetLastError(),
				"PostQueuedCompletionStatus");
		}
		break;

	case DISPATCH_MUXNOTE_HANDLE_TYPE_PIPE:
		if ((dmn->dmn_events & DISPATCH_MUXNOTE_EVENT_READ) &&
				!dmn->dmn_thread) {
			dmn->dmn_thread = (HANDLE)_beginthreadex(/* security */ NULL,
					/* stack_size */ 1, _dispatch_pipe_monitor_thread,
					(void *)dmn, /* initflag */ 0, /* thrdaddr */ NULL);
			if (!dmn->dmn_thread) {
				DISPATCH_INTERNAL_CRASH(errno, "_beginthread");
			}
			dmn->dmn_event = CreateEventW(NULL, /* bManualReset */ FALSE,
					/* bInitialState */ FALSE, NULL);
			if (!dmn->dmn_event) {
				DISPATCH_INTERNAL_CRASH(GetLastError(), "CreateEventW");
			}
		}
		if (dmn->dmn_events & DISPATCH_MUXNOTE_EVENT_WRITE) {
			_dispatch_muxnote_retain(dmn);
			DWORD available =
					_dispatch_pipe_write_availability((HANDLE)dmn->dmn_ident);
			bSuccess = PostQueuedCompletionStatus(hPort, available,
					(ULONG_PTR)DISPATCH_PORT_PIPE_HANDLE_WRITE,
					(LPOVERLAPPED)dmn);
			if (bSuccess == FALSE) {
				DISPATCH_INTERNAL_CRASH(GetLastError(),
						"PostQueuedCompletionStatus");
			}
		}
		break;

	case DISPATCH_MUXNOTE_HANDLE_TYPE_SOCKET:
		if (!dmn->dmn_event) {
			dmn->dmn_event = CreateEventW(NULL, /* bManualReset */ FALSE,
					/* bInitialState */ FALSE, NULL);
			if (!dmn->dmn_event) {
				DISPATCH_INTERNAL_CRASH(GetLastError(), "CreateEventW");
			}
		}
		if (!dmn->dmn_threadpool_wait) {
			dmn->dmn_threadpool_wait = CreateThreadpoolWait(
					_dispatch_socket_callback, dmn,
					/* PTP_CALLBACK_ENVIRON */ NULL);
			if (!dmn->dmn_threadpool_wait) {
				DISPATCH_INTERNAL_CRASH(GetLastError(), "CreateThreadpoolWait");
			}
			SetThreadpoolWait(dmn->dmn_threadpool_wait, dmn->dmn_event,
					/* pftTimeout */ NULL);
		}
		lNetworkEvents = FD_CLOSE;
		if (dmn->dmn_events & DISPATCH_MUXNOTE_EVENT_READ) {
			lNetworkEvents |= FD_READ;
		}
		if (dmn->dmn_events & DISPATCH_MUXNOTE_EVENT_WRITE) {
			lNetworkEvents |= FD_WRITE;
		}
		if (dmn->dmn_network_events != lNetworkEvents) {
			if (WSAEventSelect((SOCKET)dmn->dmn_ident, (WSAEVENT)dmn->dmn_event,
					lNetworkEvents) != 0) {
				DISPATCH_INTERNAL_CRASH(WSAGetLastError(), "WSAEventSelect");
			}
			dmn->dmn_network_events = lNetworkEvents;
		}
		if (dmn->dmn_events & DISPATCH_MUXNOTE_EVENT_WRITE) {
			// FD_WRITE is edge-triggered, not level-triggered, so it will only
			// be signaled if the socket becomes writable after a send() fails
			// with WSAEWOULDBLOCK. We can work around this by performing a
			// zero-byte send(). If the socket is writable, the send() will
			// succeed and we can immediately post a packet, and if it isn't, it
			// will fail with WSAEWOULDBLOCK and WSAEventSelect() will report
			// the next time it becomes available.
			if (send((SOCKET)dmn->dmn_ident, "", 0, 0) == 0) {
				_dispatch_muxnote_retain(dmn);
				bSuccess = PostQueuedCompletionStatus(hPort, 1,
						(ULONG_PTR)DISPATCH_PORT_SOCKET_WRITE,
						(LPOVERLAPPED)dmn);
				if (bSuccess == FALSE) {
					DISPATCH_INTERNAL_CRASH(GetLastError(),
							"PostQueuedCompletionStatus");
				}
			}
		}
		break;
	}

	return TRUE;
}

DISPATCH_ALWAYS_INLINE
static inline enum _dispatch_muxnote_events
_dispatch_unote_required_events(dispatch_unote_t du)
{
	switch (du._du->du_filter) {
	case DISPATCH_EVFILT_CUSTOM_ADD:
	case DISPATCH_EVFILT_CUSTOM_OR:
	case DISPATCH_EVFILT_CUSTOM_REPLACE:
		return 0;
	case EVFILT_WRITE:
		return DISPATCH_MUXNOTE_EVENT_WRITE;
	default:
		return DISPATCH_MUXNOTE_EVENT_READ;
	}
}

bool
_dispatch_unote_register_muxed(dispatch_unote_t du)
{
	struct dispatch_muxnote_bucket_s *dmb;
	dispatch_muxnote_t dmn;
	enum _dispatch_muxnote_events events;

	events = _dispatch_unote_required_events(du);

	dmb = _dispatch_unote_muxnote_bucket(du._du->du_ident);
	dmn = _dispatch_unote_muxnote_find(dmb, du._du->du_ident,
		du._du->du_filter);
	if (dmn) {
		WIN_PORT_ERROR();
		DISPATCH_INTERNAL_CRASH(0, "muxnote updating is not supported");
	} else {
		dmn = _dispatch_muxnote_create(du, events);
		if (!dmn) {
			return false;
		}
		if (_dispatch_io_trigger(dmn) == FALSE) {
			_dispatch_muxnote_release(dmn);
			return false;
		}
		LIST_INSERT_HEAD(dmb, dmn, dmn_list);
	}

	dispatch_unote_linkage_t dul = _dispatch_unote_get_linkage(du);
	switch (dmn->dmn_handle_type) {
	case DISPATCH_MUXNOTE_HANDLE_TYPE_INVALID:
		DISPATCH_INTERNAL_CRASH(0, "invalid handle");

	case DISPATCH_MUXNOTE_HANDLE_TYPE_FILE:
	case DISPATCH_MUXNOTE_HANDLE_TYPE_PIPE:
	case DISPATCH_MUXNOTE_HANDLE_TYPE_SOCKET:
		if (events & DISPATCH_MUXNOTE_EVENT_READ) {
			LIST_INSERT_HEAD(&dmn->dmn_readers_head, dul, du_link);
		} else if (events & DISPATCH_MUXNOTE_EVENT_WRITE) {
			LIST_INSERT_HEAD(&dmn->dmn_writers_head, dul, du_link);
		}
		break;
	}

	dul->du_muxnote = dmn;
	_dispatch_unote_state_set(du, DISPATCH_WLH_ANON, DU_STATE_ARMED);

	return true;
}

void
_dispatch_unote_resume_muxed(dispatch_unote_t du)
{
	dispatch_unote_linkage_t dul = _dispatch_unote_get_linkage(du);
	dispatch_muxnote_t dmn = dul->du_muxnote;
	dispatch_assert(_dispatch_unote_registered(du));
	_dispatch_io_trigger(dmn);
}

bool
_dispatch_unote_unregister_muxed(dispatch_unote_t du)
{
	dispatch_unote_linkage_t dul = _dispatch_unote_get_linkage(du);
	dispatch_muxnote_t dmn = dul->du_muxnote;

	switch (dmn->dmn_handle_type) {
	case DISPATCH_MUXNOTE_HANDLE_TYPE_INVALID:
		DISPATCH_INTERNAL_CRASH(0, "invalid handle");

	case DISPATCH_MUXNOTE_HANDLE_TYPE_FILE:
	case DISPATCH_MUXNOTE_HANDLE_TYPE_PIPE:
	case DISPATCH_MUXNOTE_HANDLE_TYPE_SOCKET:
		LIST_REMOVE(dul, du_link);
		_LIST_TRASH_ENTRY(dul, du_link);
		break;
	}
	dul->du_muxnote = NULL;

	LIST_REMOVE(dmn, dmn_list);
	_dispatch_muxnote_stop(dmn);
	_dispatch_muxnote_release(dmn);

	_dispatch_unote_state_set(du, DU_STATE_UNREGISTERED);
	return true;
}

static void
_dispatch_event_merge_file_handle(dispatch_muxnote_t dmn)
{
	dispatch_unote_linkage_t dul, dul_next;
	LIST_FOREACH_SAFE(dul, &dmn->dmn_readers_head, du_link, dul_next) {
		dispatch_unote_t du = _dispatch_unote_linkage_get_unote(dul);
		// consumed by dux_merge_evt()
		_dispatch_retain_unote_owner(du);
		dispatch_assert(dux_needs_rearm(du._du));
		_dispatch_unote_state_clear_bit(du, DU_STATE_ARMED);
		os_atomic_store2o(du._dr, ds_pending_data, ~1, relaxed);
		dux_merge_evt(du._du, EV_ADD | EV_ENABLE | EV_DISPATCH, 1, 0);
	}
	LIST_FOREACH_SAFE(dul, &dmn->dmn_writers_head, du_link, dul_next) {
		dispatch_unote_t du = _dispatch_unote_linkage_get_unote(dul);
		// consumed by dux_merge_evt()
		_dispatch_retain_unote_owner(du);
		dispatch_assert(dux_needs_rearm(du._du));
		_dispatch_unote_state_clear_bit(du, DU_STATE_ARMED);
		os_atomic_store2o(du._dr, ds_pending_data, ~1, relaxed);
		dux_merge_evt(du._du, EV_ADD | EV_ENABLE | EV_DISPATCH, 1, 0);
	}
	// Retained when posting the completion packet
	_dispatch_muxnote_release(dmn);
}

static void
_dispatch_event_merge_pipe_handle_read(dispatch_muxnote_t dmn,
		DWORD dwBytesAvailable)
{
	dispatch_unote_linkage_t dul, dul_next;
	LIST_FOREACH_SAFE(dul, &dmn->dmn_readers_head, du_link, dul_next) {
		dispatch_unote_t du = _dispatch_unote_linkage_get_unote(dul);
		// consumed by dux_merge_evt()
		_dispatch_retain_unote_owner(du);
		dispatch_unote_state_t du_state = _dispatch_unote_state(du);
		du_state &= ~DU_STATE_ARMED;
		uintptr_t data = dwBytesAvailable;
		uint32_t flags;
		if (dwBytesAvailable > 0) {
			flags = EV_ADD | EV_ENABLE | EV_DISPATCH;
		} else {
			du_state |= DU_STATE_NEEDS_DELETE;
			flags = EV_DELETE | EV_DISPATCH;
		}
		_dispatch_unote_state_set(du, du_state);
		os_atomic_store2o(du._dr, ds_pending_data, ~data, relaxed);
		dux_merge_evt(du._du, flags, data, 0);
	}
	SetEvent(dmn->dmn_event);
	// Retained when posting the completion packet
	_dispatch_muxnote_release(dmn);
}

static void
_dispatch_event_merge_pipe_handle_write(dispatch_muxnote_t dmn,
		DWORD dwBytesAvailable)
{
	dispatch_unote_linkage_t dul, dul_next;
	LIST_FOREACH_SAFE(dul, &dmn->dmn_writers_head, du_link, dul_next) {
		dispatch_unote_t du = _dispatch_unote_linkage_get_unote(dul);
		// consumed by dux_merge_evt()
		_dispatch_retain_unote_owner(du);
		_dispatch_unote_state_clear_bit(du, DU_STATE_ARMED);
		uintptr_t data = dwBytesAvailable;
		if (dwBytesAvailable > 0) {
			os_atomic_store2o(du._dr, ds_pending_data, ~data, relaxed);
		} else {
			os_atomic_store2o(du._dr, ds_pending_data, 0, relaxed);
		}
		dux_merge_evt(du._du, EV_ADD | EV_ENABLE | EV_DISPATCH, data, 0);
	}
	// Retained when posting the completion packet
	_dispatch_muxnote_release(dmn);
}

static void
_dispatch_event_merge_socket(dispatch_unote_t du, DWORD dwBytesAvailable)
{
	// consumed by dux_merge_evt()
	_dispatch_retain_unote_owner(du);
	dispatch_unote_state_t du_state = _dispatch_unote_state(du);
	du_state &= ~DU_STATE_ARMED;
	uintptr_t data = dwBytesAvailable;
	uint32_t flags;
	if (dwBytesAvailable > 0) {
		flags = EV_ADD | EV_ENABLE | EV_DISPATCH;
	} else {
		du_state |= DU_STATE_NEEDS_DELETE;
		flags = EV_DELETE | EV_DISPATCH;
	}
	_dispatch_unote_state_set(du, du_state);
	os_atomic_store2o(du._dr, ds_pending_data, ~data, relaxed);
	dux_merge_evt(du._du, flags, data, 0);
}

static void
_dispatch_event_merge_socket_read(dispatch_muxnote_t dmn,
		DWORD dwBytesAvailable)
{
	dispatch_unote_linkage_t dul, dul_next;
	LIST_FOREACH_SAFE(dul, &dmn->dmn_readers_head, du_link, dul_next) {
		dispatch_unote_t du = _dispatch_unote_linkage_get_unote(dul);
		_dispatch_event_merge_socket(du, dwBytesAvailable);
	}
	// Retained when posting the completion packet
	_dispatch_muxnote_release(dmn);
}

static void
_dispatch_event_merge_socket_write(dispatch_muxnote_t dmn,
		DWORD dwBytesAvailable)
{
	dispatch_unote_linkage_t dul, dul_next;
	LIST_FOREACH_SAFE(dul, &dmn->dmn_writers_head, du_link, dul_next) {
		dispatch_unote_t du = _dispatch_unote_linkage_get_unote(dul);
		_dispatch_event_merge_socket(du, dwBytesAvailable);
	}
	// Retained when posting the completion packet
	_dispatch_muxnote_release(dmn);
}

#pragma mark timers

typedef struct _dispatch_windows_timeout_s {
	PTP_TIMER pTimer;
	enum _dispatch_windows_port ullIdent;
	bool bArmed;
} *dispatch_windows_timeout_t;

#define DISPATCH_WINDOWS_TIMEOUT_INITIALIZER(clock)                             \
	[DISPATCH_CLOCK_##clock] = {                                            \
		.pTimer = NULL,                                                 \
		.ullIdent = DISPATCH_PORT_TIMER_CLOCK_##clock,                  \
		.bArmed = FALSE,                                                \
	}

static struct _dispatch_windows_timeout_s _dispatch_windows_timeout[] = {
	DISPATCH_WINDOWS_TIMEOUT_INITIALIZER(WALL),
	DISPATCH_WINDOWS_TIMEOUT_INITIALIZER(UPTIME),
	DISPATCH_WINDOWS_TIMEOUT_INITIALIZER(MONOTONIC),
};

static void
_dispatch_event_merge_timer(dispatch_clock_t clock)
{
	uint32_t tidx = DISPATCH_TIMER_INDEX(clock, 0);

	_dispatch_windows_timeout[clock].bArmed = FALSE;

	_dispatch_timers_heap_dirty(_dispatch_timers_heap, tidx);
	_dispatch_timers_heap[tidx].dth_needs_program = true;
	_dispatch_timers_heap[tidx].dth_armed = false;
}

static void CALLBACK
_dispatch_timer_callback(PTP_CALLBACK_INSTANCE Instance, PVOID Context,
	PTP_TIMER Timer)
{
	BOOL bSuccess;

	bSuccess = PostQueuedCompletionStatus(hPort, 0, (ULONG_PTR)Context,
		NULL);
	if (bSuccess == FALSE) {
		DISPATCH_INTERNAL_CRASH(GetLastError(),
			"PostQueuedCompletionStatus");
	}
}

void
_dispatch_event_loop_timer_arm(dispatch_timer_heap_t dth DISPATCH_UNUSED,
		uint32_t tidx, dispatch_timer_delay_s range,
		dispatch_clock_now_cache_t nows)
{
	dispatch_windows_timeout_t timer;
	FILETIME ftDueTime;
	LARGE_INTEGER liTime;

	switch (DISPATCH_TIMER_CLOCK(tidx)) {
	case DISPATCH_CLOCK_WALL:
		timer = &_dispatch_windows_timeout[DISPATCH_CLOCK_WALL];
		liTime.QuadPart = range.delay +
			_dispatch_time_now_cached(DISPATCH_TIMER_CLOCK(tidx), nows);
		break;

	case DISPATCH_CLOCK_UPTIME:
	case DISPATCH_CLOCK_MONOTONIC:
		timer = &_dispatch_windows_timeout[DISPATCH_TIMER_CLOCK(tidx)];
		liTime.QuadPart = -((range.delay + 99) / 100);
		break;
	}

	if (timer->pTimer == NULL) {
		timer->pTimer = CreateThreadpoolTimer(_dispatch_timer_callback,
			(LPVOID)timer->ullIdent, NULL);
		if (timer->pTimer == NULL) {
			DISPATCH_INTERNAL_CRASH(GetLastError(),
				"CreateThreadpoolTimer");
		}
	}

	ftDueTime.dwHighDateTime = liTime.HighPart;
	ftDueTime.dwLowDateTime = liTime.LowPart;

	SetThreadpoolTimer(timer->pTimer, &ftDueTime, /*msPeriod=*/0,
		/*msWindowLength=*/0);
	timer->bArmed = TRUE;
}

void
_dispatch_event_loop_timer_delete(dispatch_timer_heap_t dth DISPATCH_UNUSED,
		uint32_t tidx)
{
	dispatch_windows_timeout_t timer;

	switch (DISPATCH_TIMER_CLOCK(tidx)) {
	case DISPATCH_CLOCK_WALL:
		timer = &_dispatch_windows_timeout[DISPATCH_CLOCK_WALL];
		break;

	case DISPATCH_CLOCK_UPTIME:
	case DISPATCH_CLOCK_MONOTONIC:
		timer = &_dispatch_windows_timeout[DISPATCH_TIMER_CLOCK(tidx)];
		break;
	}

	SetThreadpoolTimer(timer->pTimer, NULL, /*msPeriod=*/0,
		/*msWindowLength=*/0);
	timer->bArmed = FALSE;
}

#pragma mark dispatch_loop

static void
_dispatch_windows_port_init(void *context DISPATCH_UNUSED)
{
	hPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	if (hPort == NULL) {
		DISPATCH_INTERNAL_CRASH(GetLastError(),
			"CreateIoCompletionPort");
	}

#if DISPATCH_USE_MGR_THREAD
	_dispatch_trace_item_push(_dispatch_mgr_q.do_targetq, &_dispatch_mgr_q);
	dx_push(_dispatch_mgr_q.do_targetq, &_dispatch_mgr_q, 0);
#endif
}

void
_dispatch_event_loop_poke(dispatch_wlh_t wlh DISPATCH_UNUSED,
		uint64_t dq_state DISPATCH_UNUSED, uint32_t flags DISPATCH_UNUSED)
{
	static dispatch_once_t _dispatch_windows_port_init_pred;
	BOOL bSuccess;

	dispatch_once_f(&_dispatch_windows_port_init_pred, NULL,
		_dispatch_windows_port_init);
	bSuccess = PostQueuedCompletionStatus(hPort, 0, DISPATCH_PORT_POKE,
		NULL);
	(void)dispatch_assume(bSuccess);
}

DISPATCH_NOINLINE
void
_dispatch_event_loop_drain(uint32_t flags)
{
	DWORD dwNumberOfBytesTransferred;
	ULONG_PTR ulCompletionKey;
	LPOVERLAPPED pOV;
	BOOL bSuccess;

	pOV = (LPOVERLAPPED)&pOV;
	bSuccess = GetQueuedCompletionStatus(hPort, &dwNumberOfBytesTransferred,
		&ulCompletionKey, &pOV,
		(flags & KEVENT_FLAG_IMMEDIATE) ? 0 : INFINITE);
	while (bSuccess) {
		switch (ulCompletionKey) {
		case DISPATCH_PORT_POKE:
			break;

		case DISPATCH_PORT_TIMER_CLOCK_WALL:
			_dispatch_event_merge_timer(DISPATCH_CLOCK_WALL);
			break;

		case DISPATCH_PORT_TIMER_CLOCK_UPTIME:
			_dispatch_event_merge_timer(DISPATCH_CLOCK_UPTIME);
			break;

		case DISPATCH_PORT_TIMER_CLOCK_MONOTONIC:
			_dispatch_event_merge_timer(DISPATCH_CLOCK_MONOTONIC);
			break;

		case DISPATCH_PORT_FILE_HANDLE:
			_dispatch_event_merge_file_handle((dispatch_muxnote_t)pOV);
			break;

		case DISPATCH_PORT_PIPE_HANDLE_READ:
			_dispatch_event_merge_pipe_handle_read((dispatch_muxnote_t)pOV,
					dwNumberOfBytesTransferred);
			break;

		case DISPATCH_PORT_PIPE_HANDLE_WRITE:
			_dispatch_event_merge_pipe_handle_write((dispatch_muxnote_t)pOV,
					dwNumberOfBytesTransferred);
			break;

		case DISPATCH_PORT_SOCKET_READ:
			_dispatch_event_merge_socket_read((dispatch_muxnote_t)pOV,
					dwNumberOfBytesTransferred);
			break;

		case DISPATCH_PORT_SOCKET_WRITE:
			_dispatch_event_merge_socket_write((dispatch_muxnote_t)pOV,
					dwNumberOfBytesTransferred);
			break;

		default:
			DISPATCH_INTERNAL_CRASH(ulCompletionKey,
				"unsupported completion key");
		}

		bSuccess = GetQueuedCompletionStatus(hPort,
			&dwNumberOfBytesTransferred, &ulCompletionKey, &pOV, 0);
	}

	if (bSuccess == FALSE && pOV != NULL) {
		DISPATCH_INTERNAL_CRASH(GetLastError(),
			"GetQueuedCompletionStatus");
	}
}

void
_dispatch_event_loop_cancel_waiter(dispatch_sync_context_t dsc DISPATCH_UNUSED)
{
	WIN_PORT_ERROR();
}

void
_dispatch_event_loop_wake_owner(dispatch_sync_context_t dsc,
		dispatch_wlh_t wlh, uint64_t old_state, uint64_t new_state)
{
	(void)dsc; (void)wlh; (void)old_state; (void)new_state;
}

void
_dispatch_event_loop_wait_for_ownership(dispatch_sync_context_t dsc)
{
	if (dsc->dsc_release_storage) {
		_dispatch_queue_release_storage(dsc->dc_data);
	}
}

void
_dispatch_event_loop_end_ownership(dispatch_wlh_t wlh, uint64_t old_state,
		uint64_t new_state, uint32_t flags)
{
	(void)wlh; (void)old_state; (void)new_state; (void)flags;
}

#if DISPATCH_WLH_DEBUG
void
_dispatch_event_loop_assert_not_owned(dispatch_wlh_t wlh)
{
	(void)wlh;
}
#endif

void
_dispatch_event_loop_leave_immediate(uint64_t dq_state)
{
	(void)dq_state;
}

#endif // DISPATCH_EVENT_BACKEND_WINDOWS
