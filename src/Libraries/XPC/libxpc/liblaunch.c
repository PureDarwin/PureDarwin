/*
 * Copyright (c) 2005-2012 Apple Inc. All rights reserved.
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

// Turn off deprecation warnings so we can see what eles is wrong
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include "config.h"
#include "launch.h"
#include "launch_priv.h"
#include "launch_internal.h"
#include "ktrace.h"

#include <mach/mach.h>
#include <libkern/OSByteOrder.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <assert.h>
#include <uuid/uuid.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include <xpc/xpc.h>
#include "nv.h"

extern void xpc_api_misuse(const char *reason, ...) __dead2;
extern const char *_xpc_get_type_name(xpc_object_t obj);
struct xpc_object;
extern nvlist_t *xpc2nv(struct xpc_object *xo, int64_t (^port_serializer)(mach_port_t port));
extern struct xpc_object *nv2xpc(const nvlist_t *nv, mach_port_t (^port_deserializer)(int64_t port_id));

typedef union {
	uint64_t ui;
	int64_t i;
	const char *str;
	bool b;
	double d;
	uintptr_t ptr;
	int fd;
	uuid_t uuid;
	mach_port_t port;
} pd_launch_xpc_u;

struct pd_launch_xpc_object {
	const void *isa;
	int ref_cnt;
	int xref_cnt;
	xpc_type_t xo_xpc_type;
	uint16_t xo_flags;
	size_t xo_size;
	pd_launch_xpc_u xo_u;
};

static void
pd_liblaunch_phase(const char *fmt, ...)
{
	return; /* PD-DIAG boot traces silenced; remove to re-enable */
	int fd;
	va_list ap;
	char buf[1024];

	fd = open("/dev/console", O_WRONLY | O_NOCTTY);
	if (fd < 0) {
		return;
	}
	dprintf(fd, "PureDarwin liblaunch: ");
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	write(fd, buf, strlen(buf));
	dprintf(fd, "\n");
	close(fd);
}

static const char *
pd_launch_xpc_type_name(launch_data_t d)
{
	if (d == NULL) {
		return "(null)";
	}

	return _xpc_get_type_name(d);
}

static bool
pd_launch_xpc_type_is(launch_data_t d, xpc_type_t local_type, const char *type_name)
{
	xpc_type_t type;
	const char *name;

	if (d == NULL) {
		return false;
	}

	type = xpc_get_type(d);
	if (type == local_type) {
		return true;
	}

	name = _xpc_get_type_name(d);
	return name && strcmp(name, type_name) == 0;
}

#ifdef __LP64__
/* workaround: 5723161 */
#ifndef __DARWIN_ALIGN32
#define	__DARWIN_ALIGN32(x)	(((size_t)(x) + 3) & ~3)
#endif
#undef	CMSG_DATA
#define	CMSG_DATA(cmsg)	\
	((uint8_t *)(cmsg) + __DARWIN_ALIGN32(sizeof(struct cmsghdr)))
#undef	CMSG_SPACE
#define	CMSG_SPACE(l)	\
	(__DARWIN_ALIGN32(sizeof(struct cmsghdr)) + __DARWIN_ALIGN32(l))
#undef	CMSG_LEN
#define	CMSG_LEN(l)	\
	(__DARWIN_ALIGN32(sizeof(struct cmsghdr)) + (l))
#endif

#include "bootstrap.h"
#include "vproc.h"
#include "vproc_priv.h"
#include "vproc_internal.h"

/* __OSBogusByteSwap__() must not really exist in the symbol namespace
 * in order for the following to generate an error at build time.
 */
extern void __OSBogusByteSwap__(void);

#define host2wire(x)				\
	({ typeof (x) _X, _x = (x);		\
	 switch (sizeof(_x)) {			\
	 case 8:				\
	 	_X = OSSwapHostToLittleInt64(_x);	\
	 	break;				\
	 case 4:				\
	 	_X = OSSwapHostToLittleInt32(_x);	\
	 	break;				\
	 case 2:				\
	 	_X = OSSwapHostToLittleInt16(_x);	\
	 	break;				\
	 case 1:				\
	 	_X = _x;			\
		break;				\
	 default:				\
	 	__OSBogusByteSwap__();		\
		break;				\
	 }					\
	 _X;					\
	 })


#define big2wire(x)				\
	({ typeof (x) _X, _x = (x);		\
	 switch (sizeof(_x)) {			\
	 case 8:				\
	 	_X = OSSwapLittleToHostInt64(_x);	\
	 	break;				\
	 case 4:				\
	 	_X = OSSwapLittleToHostInt32(_x);	\
	 	break;				\
	 case 2:				\
	 	_X = OSSwapLittleToHostInt16(_x);	\
	 	break;				\
	 case 1:				\
	 	_X = _x;			\
		break;				\
	 default:				\
	 	__OSBogusByteSwap__();		\
		break;				\
	 }					\
	 _X;					\
	 })

union _launch_double_u {
	uint64_t iv;
	double dv;
};

#define host2wire_f(x) ({ \
	typeof(x) _F, _f = (x); \
	union _launch_double_u s; \
	s.dv = _f; \
	s.iv = host2wire(s.iv); \
	_F = s.dv; \
	_F; \
})

#define big2wire_f(x) ({ \
	typeof(x) _F, _f = (x); \
	union _launch_double_u s; \
	s.dv = _f; \
	s.iv = big2wire(s.iv); \
	_F = s.dv; \
	_F; \
})


struct launch_msg_header {
	uint64_t magic;
	uint64_t len;
};

#define LAUNCH_MSG_HEADER_MAGIC 0xD2FEA02366B39A41ull

enum {
	LAUNCHD_USE_CHECKIN_FD,
	LAUNCHD_USE_OTHER_FD,
};
struct _launch {
	void	*sendbuf;
	int	*sendfds;
	void	*recvbuf;
	int	*recvfds;
	size_t	sendlen;
	size_t	sendfdcnt;
	size_t	recvlen;
	size_t	recvfdcnt;
	int which;
	int cifd;
	int	fd;
};

static launch_data_t launch_data_array_pop_first(launch_data_t where);
static int _fd(int fd);
static void launch_client_init(void);
static void launch_msg_getmsgs(launch_data_t m, void *context);
static launch_data_t launch_msg_internal(launch_data_t d);
static void launch_mach_checkin_service(launch_data_t obj, const char *key, void *context);

void
_launch_init_globals(launch_globals_t globals)
{
	pthread_once_t once = PTHREAD_ONCE_INIT;
	globals->lc_once = once;
	pthread_mutex_init(&globals->lc_mtx, NULL);
}

#if !_LIBLAUNCH_HAS_ALLOC_ONCE
launch_globals_t __launch_globals;

void
_launch_globals_init(void)
{
	__launch_globals = calloc(1, sizeof(struct launch_globals_s));
	_launch_init_globals(__launch_globals);
}

launch_globals_t
_launch_globals_impl(void)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	pthread_once(&once, &_launch_globals_init);
	return __launch_globals;
}
#endif

void
launch_client_init(void)
{
	struct sockaddr_un sun;
	char *where = getenv(LAUNCHD_SOCKET_ENV);
	char *_launchd_fd = getenv(LAUNCHD_TRUSTED_FD_ENV);
	int dfd, lfd = -1, cifd = -1;
	name_t spath;

	if (_launchd_fd) {
		cifd = strtol(_launchd_fd, NULL, 10);
		if ((dfd = dup(cifd)) >= 0) {
			close(dfd);
			_fd(cifd);
		} else {
			cifd = -1;
		}
		unsetenv(LAUNCHD_TRUSTED_FD_ENV);
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;

	/* The rules are as follows. 
	 * - All users (including root) talk to their per-user launchd's by default.
	 * - If we have been invoked under sudo, talk to the system launchd.
	 * - If we're the root user and the __USE_SYSTEM_LAUNCHD environment variable is set, then
	 *   talk to the system launchd.
	 */
	if (where && where[0] != '\0') {
		strncpy(sun.sun_path, where, sizeof(sun.sun_path));
	} else {
		if (_vprocmgr_getsocket(spath) == 0) {
			if ((getenv("SUDO_COMMAND") || getenv("__USE_SYSTEM_LAUNCHD")) && geteuid() == 0) {
				/* Talk to the system launchd. */
				strncpy(sun.sun_path, LAUNCHD_SOCK_PREFIX "/sock", sizeof(sun.sun_path));
			} else {
				/* Talk to our per-user launchd. */
				size_t min_len;

				min_len = sizeof(sun.sun_path) < sizeof(spath) ? sizeof(sun.sun_path) : sizeof(spath);

				strncpy(sun.sun_path, spath, min_len);
			}
		} else if ((getenv("SUDO_COMMAND") || getenv("__USE_SYSTEM_LAUNCHD")) && geteuid() == 0) {
			/* Early PID 1 bootstrappers may not be able to ask vproc for the
			 * socket yet, but the system launchd listener is already fixed. */
			strncpy(sun.sun_path, LAUNCHD_SOCK_PREFIX "/sock", sizeof(sun.sun_path));
		}
	}

	launch_globals_t globals = _launch_globals();
	if ((lfd = _fd(socket(AF_UNIX, SOCK_STREAM, 0))) == -1) {
		goto out_bad;
	}

#if TARGET_OS_EMBEDDED
	(void)vproc_swap_integer(NULL, VPROC_GSK_EMBEDDEDROOTEQUIVALENT, NULL, &globals->s_am_embedded_god);
#endif
	if (-1 == connect(lfd, (struct sockaddr *)&sun, sizeof(sun))) {
		pd_liblaunch_phase("connect %s failed errno=%d", sun.sun_path, errno);
		if (cifd != -1 || globals->s_am_embedded_god) {
			/* There is NO security enforced by this check. This is just a hint to our
			 * library that we shouldn't error out due to failing to open this socket. If
			 * we inherited a trusted file descriptor, we shouldn't fail. This should be
			 * adequate for clients' expectations.
			 */
			close(lfd);
			lfd = -1;
		} else {
			goto out_bad;
		}
	}
	
	if (!(globals->l = launchd_fdopen(lfd, cifd))) {
		goto out_bad;
	}

	if (!(globals->async_resp = launch_data_alloc(LAUNCH_DATA_ARRAY))) {
		goto out_bad;
	}

	return;
out_bad:
	if (globals->l) {
		launchd_close(globals->l, close);
		globals->l = NULL;
	} else if (lfd != -1) {
		close(lfd);
	}
	if (cifd != -1) {
		close(cifd);
	}
}

extern xpc_object_t xpc_bool_create_distinct(bool value);
launch_data_t
launch_data_alloc(launch_data_type_t t)
{
	if (t == LAUNCH_DATA_DICTIONARY) return (launch_data_t)xpc_dictionary_create(NULL, NULL, 0);
	else if (t == LAUNCH_DATA_ARRAY) return (launch_data_t)xpc_array_create(NULL, 0);
	else if (t == LAUNCH_DATA_OPAQUE) return xpc_data_create(NULL, 0);
	else if (t == LAUNCH_DATA_STRING) return xpc_string_create("");
	else if (t == LAUNCH_DATA_BOOL) return xpc_bool_create_distinct(false);
	else if (t == LAUNCH_DATA_INTEGER) return xpc_int64_create(0);
	else if (t == LAUNCH_DATA_REAL) return xpc_double_create(0);
	else if (t == LAUNCH_DATA_ERRNO) return xpc_uint64_create(0);
	/* Legacy launchd's `launchctl list` reply carries LAUNCH_DATA_MACHPORT /
	 * LAUNCH_DATA_FD entries (job_export -> launch_data_new_machport /
	 * launch_data_new_fd). Modern libxpc dropped these types, but stubbing the
	 * alloc to xpc_api_misuse() aborts - and when that happens inside PID 1
	 * (any `launchctl list` that hits a job with MachServices, e.g. notifyd)
	 * the kernel panics on launchd's death. The list path only ever stores
	 * MACH_PORT_NULL / a placeholder fd, so back both with a plain int64. */
	else if (t == LAUNCH_DATA_MACHPORT) return xpc_int64_create(0);
	else if (t == LAUNCH_DATA_FD) return xpc_int64_create(-1);
	else xpc_api_misuse("You cannot create a launch_data_t of type %d anymore", t);
}

launch_data_type_t
launch_data_get_type(launch_data_t d)
{
	xpc_type_t type = xpc_get_type(d);
	const char *name = pd_launch_xpc_type_name(d);

	if (type == XPC_TYPE_DICTIONARY || strcmp(name, "dictionary") == 0) return LAUNCH_DATA_DICTIONARY;
	else if (type == XPC_TYPE_ARRAY || strcmp(name, "array") == 0) return LAUNCH_DATA_ARRAY;
	else if (type == XPC_TYPE_FD || strcmp(name, "file descriptor") == 0) return LAUNCH_DATA_FD;
	else if (type == XPC_TYPE_INT64 || strcmp(name, "int64") == 0) return LAUNCH_DATA_INTEGER;
	else if (type == XPC_TYPE_UINT64 || strcmp(name, "uint64") == 0) return LAUNCH_DATA_ERRNO;
	else if (type == XPC_TYPE_DOUBLE || strcmp(name, "double") == 0) return LAUNCH_DATA_REAL;
	else if (type == XPC_TYPE_BOOL || strcmp(name, "bool") == 0) return LAUNCH_DATA_BOOL;
	else if (type == XPC_TYPE_DATA || strcmp(name, "data") == 0) return LAUNCH_DATA_OPAQUE;
	else if (type == XPC_TYPE_STRING || strcmp(name, "string") == 0) return LAUNCH_DATA_STRING;
	else if (type == XPC_TYPE_UUID || strcmp(name, "UUID") == 0) return LAUNCH_DATA_OPAQUE;
	else if (type == XPC_TYPE_ERROR || strcmp(name, "error") == 0) return LAUNCH_DATA_ERRNO;
	else if (type == XPC_TYPE_ENDPOINT || strcmp(name, "endpoint") == 0) return LAUNCH_DATA_MACHPORT;

	pd_liblaunch_phase("launch_data_get_type unknown object=%p type=%p name=%s", d, type,
	    name);
	xpc_api_misuse("Unknown xpc_type_t %p, this should not happen", type);
}

void
launch_data_free(launch_data_t d)
{
	xpc_release(d);
}

size_t
launch_data_dict_get_count(launch_data_t dict)
{
	if (!pd_launch_xpc_type_is(dict, XPC_TYPE_DICTIONARY, "dictionary")) {
		errno = EINVAL;
		return 0;
	}
	return xpc_dictionary_get_count(dict);
}

bool
launch_data_dict_insert(launch_data_t dict, launch_data_t what, const char *key)
{
	if (!pd_launch_xpc_type_is(dict, XPC_TYPE_DICTIONARY, "dictionary")) {
		pd_liblaunch_phase("launch_data_dict_insert skipped non-dictionary object=%p type=%s key=%s",
				dict, pd_launch_xpc_type_name(dict), key ? key : "(null)");
		errno = EINVAL;
		return false;
	}
	xpc_dictionary_set_value(dict, key, what);
	return true;
}

launch_data_t
launch_data_dict_lookup(launch_data_t dict, const char *key)
{
	if (!pd_launch_xpc_type_is(dict, XPC_TYPE_DICTIONARY, "dictionary")) {
		errno = EINVAL;
		return NULL;
	}
	return xpc_dictionary_get_value(dict, key);
}

bool
launch_data_dict_remove(launch_data_t dict, const char *key)
{
	if (!pd_launch_xpc_type_is(dict, XPC_TYPE_DICTIONARY, "dictionary")) {
		errno = EINVAL;
		return false;
	}
	xpc_dictionary_set_value(dict, key, NULL);
	return true;
}

void
launch_data_dict_iterate(launch_data_t dict, void (*cb)(launch_data_t, const char *, void *), void *context)
{
	if (!pd_launch_xpc_type_is(dict, XPC_TYPE_DICTIONARY, "dictionary")) {
		errno = EINVAL;
		return;
	}
	xpc_dictionary_apply(dict, ^bool(const char *key, xpc_object_t value) {
		cb(value, key, context);
		return true;
	});
}

bool
launch_data_array_set_index(launch_data_t where, launch_data_t what, size_t ind)
{
	if (!pd_launch_xpc_type_is(where, XPC_TYPE_ARRAY, "array")) {
		pd_liblaunch_phase("launch_data_array_set_index skipped non-array object=%p type=%s ind=%zu",
				where, pd_launch_xpc_type_name(where), ind);
		errno = EINVAL;
		return false;
	}
	xpc_array_set_value(where, ind, what);
	return true;
}

launch_data_t
launch_data_array_get_index(launch_data_t where, size_t ind)
{
	if (!pd_launch_xpc_type_is(where, XPC_TYPE_ARRAY, "array")) {
		errno = EINVAL;
		return NULL;
	}
	return xpc_array_get_value(where, ind);
}

launch_data_t
launch_data_array_pop_first(launch_data_t where)
{
	xpc_object_t copy = xpc_array_create(NULL, 0);

	__block bool first = true;
	xpc_array_apply(where, ^bool(size_t index, xpc_object_t value) {
		if (first) {
			first = false;
		} else {
			xpc_array_append_value(copy, value);
		}

		return true;
	});

	xpc_release(where);
	xpc_retain(copy);

	return copy;
}

size_t
launch_data_array_get_count(launch_data_t where)
{
	return xpc_array_get_count(where);
}

extern void xpc_uint64_set_value(xpc_object_t xo, uint64_t value);
extern void xpc_int64_set_value(xpc_object_t xo, uint64_t value);

bool
launch_data_set_errno(launch_data_t d, int e)
{
	xpc_uint64_set_value(d, e);
	return true;
}

bool
launch_data_set_fd(launch_data_t d, int fd)
{
	/* See launch_data_alloc: FD launch_data is backed by an int64 placeholder
	 * for the legacy `launchctl list` reply path. */
	xpc_int64_set_value(d, fd);
	return true;
}

bool
launch_data_set_machport(launch_data_t d, mach_port_t p)
{
	/* See launch_data_alloc: MACHPORT launch_data is backed by an int64
	 * placeholder for the legacy `launchctl list` reply path. */
	xpc_int64_set_value(d, p);
	return true;
}

bool
launch_data_set_integer(launch_data_t d, long long n)
{
	xpc_int64_set_value(d, n);
	return true;
}

extern void xpc_bool_set_value(xpc_object_t xo, bool value);

bool
launch_data_set_bool(launch_data_t d, bool b)
{
	xpc_bool_set_value(d, b);
	return true;
}

extern void xpc_double_set_value(xpc_object_t xo, double value);

bool
launch_data_set_real(launch_data_t d, double n)
{
	xpc_double_set_value(d, n);
	return true;
}

extern void xpc_string_set_value(xpc_object_t xo, const char *value);

bool
launch_data_set_string(launch_data_t d, const char *s)
{
	xpc_string_set_value(d, s);
	return true;
}

extern void xpc_data_set_bytes(xpc_object_t xo, void *buffer, size_t size);

bool
launch_data_set_opaque(launch_data_t d, const void *o, size_t os)
{
	xpc_data_set_bytes(d, o, os);
	return true;
}

int
launch_data_get_errno(launch_data_t d)
{
	if (pd_launch_xpc_type_is(d, XPC_TYPE_UINT64, "uint64"))
		return ((struct pd_launch_xpc_object *)d)->xo_u.ui;

	return xpc_uint64_get_value(d);
}

int
launch_data_get_fd(launch_data_t d)
{
	return xpc_fd_dup(d);
}

extern mach_port_t xpc_object_get_machport(xpc_object_t obj);

mach_port_t
launch_data_get_machport(launch_data_t d)
{
	return xpc_object_get_machport(d);
}

long long
launch_data_get_integer(launch_data_t d)
{
	if (pd_launch_xpc_type_is(d, XPC_TYPE_INT64, "int64"))
		return ((struct pd_launch_xpc_object *)d)->xo_u.i;

	return xpc_int64_get_value(d);
}

bool
launch_data_get_bool(launch_data_t d)
{
	if (pd_launch_xpc_type_is(d, XPC_TYPE_BOOL, "bool"))
		return ((struct pd_launch_xpc_object *)d)->xo_u.b;

	return xpc_bool_get_value(d);
}

double
launch_data_get_real(launch_data_t d)
{
	if (pd_launch_xpc_type_is(d, XPC_TYPE_DOUBLE, "double"))
		return ((struct pd_launch_xpc_object *)d)->xo_u.d;

	return xpc_double_get_value(d);
}

const char *
launch_data_get_string(launch_data_t d)
{
	if (pd_launch_xpc_type_is(d, XPC_TYPE_STRING, "string"))
		return ((struct pd_launch_xpc_object *)d)->xo_u.str;

	return xpc_string_get_string_ptr(d);
}

void *
launch_data_get_opaque(launch_data_t d)
{
	if (pd_launch_xpc_type_is(d, XPC_TYPE_UUID, "UUID"))
		return (void *)&((struct pd_launch_xpc_object *)d)->xo_u.uuid;

	if (pd_launch_xpc_type_is(d, XPC_TYPE_DATA, "data"))
		return (void *)((struct pd_launch_xpc_object *)d)->xo_u.ptr;

	return xpc_data_get_bytes_ptr(d);
}

size_t
launch_data_get_opaque_size(launch_data_t d)
{
	if (pd_launch_xpc_type_is(d, XPC_TYPE_UUID, "UUID"))
		return sizeof(uuid_t);

	if (pd_launch_xpc_type_is(d, XPC_TYPE_DATA, "data"))
		return ((struct pd_launch_xpc_object *)d)->xo_size;

	return xpc_data_get_length(d);
}

int
launchd_getfd(launch_t l)
{
	return (l->which == LAUNCHD_USE_CHECKIN_FD) ? l->cifd : l->fd;
}

launch_t
launchd_fdopen(int fd, int cifd)
{
	launch_t c;

	c = calloc(1, sizeof(struct _launch));
	if (!c)
		return NULL;

	c->fd = fd;
	c->cifd = cifd;

	if (c->fd == -1 || (c->fd != -1 && c->cifd != -1)) {
		c->which = LAUNCHD_USE_CHECKIN_FD;
	} else if (c->cifd == -1) {
		c->which = LAUNCHD_USE_OTHER_FD;
	}

	fcntl(fd, F_SETFL, O_NONBLOCK);
	fcntl(cifd, F_SETFL, O_NONBLOCK);

	if ((c->sendbuf = malloc(0)) == NULL)
		goto out_bad;
	if ((c->sendfds = malloc(0)) == NULL)
		goto out_bad;
	if ((c->recvbuf = malloc(0)) == NULL)
		goto out_bad;
	if ((c->recvfds = malloc(0)) == NULL)
		goto out_bad;

	return c;

out_bad:
	if (c->sendbuf)
		free(c->sendbuf);
	if (c->sendfds)
		free(c->sendfds);
	if (c->recvbuf)
		free(c->recvbuf);
	if (c->recvfds)
		free(c->recvfds);
	free(c);
	return NULL;
}

void
launchd_close(launch_t lh, typeof(close) closefunc)
{
	launch_globals_t globals = _launch_globals();

	if (globals->in_flight_msg_recv_client == lh) {
		globals->in_flight_msg_recv_client = NULL;
	}

	if (lh->sendbuf)
		free(lh->sendbuf);
	if (lh->sendfds)
		free(lh->sendfds);
	if (lh->recvbuf)
		free(lh->recvbuf);
	if (lh->recvfds)
		free(lh->recvfds);
	closefunc(lh->fd);
	closefunc(lh->cifd);
	free(lh);
}

#define ROUND_TO_64BIT_WORD_SIZE(x)	((x + 7) & ~7)

size_t
launch_data_pack(launch_data_t d, void *where, size_t len, int *fd_where, size_t *fd_cnt)
{
	if (!pd_launch_xpc_type_is(d, XPC_TYPE_DICTIONARY, "dictionary") &&
			!pd_launch_xpc_type_is(d, XPC_TYPE_ARRAY, "array")) {
		pd_liblaunch_phase("launch_data_pack refused non-container object=%p type=%s",
				d, pd_launch_xpc_type_name(d));
		errno = EINVAL;
		return 0;
	}

	nvlist_t *nvl = xpc2nv((struct xpc_object *)d, ^int64_t(mach_port_t port) {
		xpc_api_misuse("Cannot currently serialize mach ports in launch_data_pack()");
	});

	size_t size;
	void *data = nvlist_pack(nvl, &size);
	if (data && where && size <= len) {
		memcpy(where, data, size);
	}

	free(data);
	nvlist_destroy(nvl);

	return size;
}

launch_data_t
launch_data_unpack(void *data, size_t data_size, int *fds, size_t fd_cnt, size_t *data_offset, size_t *fdoffset)
{
	size_t offset = data_offset ? *data_offset : 0;
	if (offset > data_size) {
		return NULL;
	}

	nvlist_t *nvl = nvlist_unpack((char *)data + offset, data_size - offset);
	if (nvl == NULL) {
		return NULL;
	}
	xpc_object_t xo = (xpc_object_t)nv2xpc(nvl, ^mach_port_t(int64_t port_id) {
		xpc_api_misuse("Should not be called");
	});
	nvlist_destroy(nvl);
	if (data_offset) {
		*data_offset = data_size;
	}
	if (fdoffset) {
		*fdoffset = 0;
	}
	return xo;
}

int
launchd_msg_send(launch_t lh, launch_data_t d)
{
	struct launch_msg_header lmh;
	struct cmsghdr *cm = NULL;
	struct msghdr mh;
	struct iovec iov[2];
	size_t sentctrllen = 0;
	int r;

	int fd2use = launchd_getfd(lh);
	if (fd2use == -1) {
		errno = EPERM;
		return -1;
	}

	memset(&mh, 0, sizeof(mh));

	/* confirm that the next hack works */
	assert((d && lh->sendlen == 0) || (!d && lh->sendlen));

	if (d) {
		size_t fd_slots_used = 0;
		size_t good_enough_size = 10 * 1024 * 1024;
		uint64_t msglen;

		/* hack, see the above assert to verify "correctness" */
		free(lh->sendbuf);
		lh->sendbuf = malloc(good_enough_size);
		if (!lh->sendbuf) {
			errno = ENOMEM;
			return -1;
		}

		free(lh->sendfds);
		lh->sendfds = malloc(4 * 1024);
		if (!lh->sendfds) {
			free(lh->sendbuf);
			lh->sendbuf = NULL;
			errno = ENOMEM;
			return -1;
		}

		lh->sendlen = launch_data_pack(d, lh->sendbuf, good_enough_size, lh->sendfds, &fd_slots_used);

		if (lh->sendlen == 0) {
			errno = ENOMEM;
			return -1;
		}

		lh->sendfdcnt = fd_slots_used;

		msglen = lh->sendlen + sizeof(struct launch_msg_header); /* type promotion to make the host2wire() macro work right */
		lmh.len = host2wire(msglen);
		lmh.magic = host2wire(LAUNCH_MSG_HEADER_MAGIC);

		iov[0].iov_base = &lmh;
		iov[0].iov_len = sizeof(lmh);
		mh.msg_iov = iov;
		mh.msg_iovlen = 2;
	} else {
		mh.msg_iov = iov + 1;
		mh.msg_iovlen = 1;
	}

	iov[1].iov_base = lh->sendbuf;
	iov[1].iov_len = lh->sendlen;


	if (lh->sendfdcnt > 0) {
		sentctrllen = mh.msg_controllen = CMSG_SPACE(lh->sendfdcnt * sizeof(int));
		cm = alloca(mh.msg_controllen);
		mh.msg_control = cm;

		memset(cm, 0, mh.msg_controllen);

		cm->cmsg_len = CMSG_LEN(lh->sendfdcnt * sizeof(int));
		cm->cmsg_level = SOL_SOCKET;
		cm->cmsg_type = SCM_RIGHTS;

		memcpy(CMSG_DATA(cm), lh->sendfds, lh->sendfdcnt * sizeof(int));
	}

	if ((r = sendmsg(fd2use, &mh, 0)) == -1) {
		return -1;
	} else if (r == 0) {
		errno = ECONNRESET;
		return -1;
	} else if (sentctrllen != mh.msg_controllen) {
		errno = ECONNRESET;
		return -1;
	}

	if (d) {
		r -= sizeof(struct launch_msg_header);
	}

	lh->sendlen -= r;
	if (lh->sendlen > 0) {
		memmove(lh->sendbuf, lh->sendbuf + r, lh->sendlen);
	} else {
		free(lh->sendbuf);
		lh->sendbuf = malloc(0);
	}

	lh->sendfdcnt = 0;
	free(lh->sendfds);
	lh->sendfds = malloc(0);

	if (lh->sendlen > 0) {
		errno = EAGAIN;
		return -1;
	}

	return 0;
}

int
launch_get_fd(void)
{
	launch_globals_t globals = _launch_globals();
	pthread_once(&globals->lc_once, launch_client_init);

	if (!globals->l) {
		errno = ENOTCONN;
		return -1;
	}

	return globals->l->fd;
}

void
launch_msg_getmsgs(launch_data_t m, void *context)
{
	launch_data_t async_resp, *sync_resp = context;

	launch_globals_t globals = _launch_globals();

	if ((LAUNCH_DATA_DICTIONARY == launch_data_get_type(m)) && (async_resp = launch_data_dict_lookup(m, LAUNCHD_ASYNC_MSG_KEY))) {
		launch_data_array_set_index(globals->async_resp, launch_data_copy(async_resp), launch_data_array_get_count(globals->async_resp));
	} else {
		*sync_resp = launch_data_copy(m);
	}
}

void
launch_mach_checkin_service(launch_data_t obj, const char *key, void *context __attribute__((unused)))
{
	kern_return_t result;
	mach_port_t p;
	name_t srvnm;

	strlcpy(srvnm, key, sizeof(srvnm));

	result = bootstrap_check_in(bootstrap_port, srvnm, &p);

	if (result == BOOTSTRAP_SUCCESS)
		launch_data_set_machport(obj, p);
}

launch_data_t
launch_msg(launch_data_t d)
{
	launch_data_t mps, r = launch_msg_internal(d);

	if (launch_data_get_type(d) == LAUNCH_DATA_STRING) {
		if (strcmp(launch_data_get_string(d), LAUNCH_KEY_CHECKIN) != 0)
			return r;
		if (r == NULL)
			return r;
		if (launch_data_get_type(r) != LAUNCH_DATA_DICTIONARY)
			return r;
		mps = launch_data_dict_lookup(r, LAUNCH_JOBKEY_MACHSERVICES);
		if (mps == NULL)
			return r;
		launch_data_dict_iterate(mps, launch_mach_checkin_service, NULL);
	}

	return r;
}

extern kern_return_t vproc_mig_set_security_session(mach_port_t, uuid_t, mach_port_t);

static inline bool
uuid_data_is_null(launch_data_t d)
{
	bool result = false;
	if (launch_data_get_type(d) == LAUNCH_DATA_OPAQUE && launch_data_get_opaque_size(d) == sizeof(uuid_t)) {
		uuid_t existing_uuid;
		memcpy(existing_uuid, launch_data_get_opaque(d), sizeof(uuid_t));

		/* A NULL UUID tells us to keep the session inherited from the parent. */
		result = (bool)uuid_is_null(existing_uuid);
	}

	return result;
}

launch_data_t
launch_msg_internal(launch_data_t d)
{
	launch_data_t resp = NULL;

	if (d && (launch_data_get_type(d) == LAUNCH_DATA_STRING)
			&& (strcmp(launch_data_get_string(d), LAUNCH_KEY_GETJOBS) == 0)
			&& vproc_swap_complex(NULL, VPROC_GSK_ALLJOBS, NULL, &resp) == NULL) {
		return resp;
	}

	launch_globals_t globals = _launch_globals();
	pthread_once(&globals->lc_once, launch_client_init);
	if (!globals->l) {
		errno = ENOTCONN;
		return NULL;
	}

	int fd2use = -1;
	if ((launch_data_get_type(d) == LAUNCH_DATA_STRING && strcmp(launch_data_get_string(d), LAUNCH_KEY_CHECKIN) == 0) || globals->s_am_embedded_god) {
		globals->l->which = LAUNCHD_USE_CHECKIN_FD;
	} else {
		globals->l->which = LAUNCHD_USE_OTHER_FD;
	}

	fd2use = launchd_getfd(globals->l);

	if (fd2use == -1) {
		errno = EPERM;
		return NULL;
	}

#if !TARGET_OS_EMBEDDED
	uuid_t uuid;
	launch_data_t uuid_d = NULL;
	size_t jobs_that_need_sessions = 0;
	if (d && launch_data_get_type(d) == LAUNCH_DATA_DICTIONARY) {
		launch_data_t v = launch_data_dict_lookup(d, LAUNCH_KEY_SUBMITJOB);

		if (v && launch_data_get_type(v) == LAUNCH_DATA_ARRAY) {
			size_t cnt = launch_data_array_get_count(v);
			size_t i = 0;

			pd_liblaunch_phase("SubmitJob array count=%zu before uuid", cnt);
			uuid_generate(uuid);
			for (i = 0; i < cnt; i++) {
				launch_data_t ji = launch_data_array_get_index(v, i);
				launch_data_type_t ji_type = ji ? launch_data_get_type(ji) : 0;
				launch_data_t label = ji_type == LAUNCH_DATA_DICTIONARY ? launch_data_dict_lookup(ji, LAUNCH_JOBKEY_LABEL) : NULL;
				pd_liblaunch_phase("SubmitJob job[%zu]=%s type=%d before uuid insert",
						i, label ? launch_data_get_string(label) : "(no label)",
						ji ? ji_type : -1);
				if (ji_type == LAUNCH_DATA_DICTIONARY) {
					launch_data_t existing_v = launch_data_dict_lookup(ji, LAUNCH_JOBKEY_SECURITYSESSIONUUID);
					if (!existing_v) {
						/* I really wish these were reference-counted. Sigh... */
						uuid_d = launch_data_new_opaque(uuid, sizeof(uuid));
						launch_data_dict_insert(ji, uuid_d, LAUNCH_JOBKEY_SECURITYSESSIONUUID);
						jobs_that_need_sessions++;
					} else if (launch_data_get_type(existing_v) == LAUNCH_DATA_OPAQUE) {
						jobs_that_need_sessions += uuid_data_is_null(existing_v) ? 0 : 1;
					}
				}
			}
		} else if (v && launch_data_get_type(v) == LAUNCH_DATA_DICTIONARY) {
			launch_data_t existing_v = launch_data_dict_lookup(v, LAUNCH_JOBKEY_SECURITYSESSIONUUID);
			if (!existing_v) {
				uuid_generate(uuid);
				uuid_d = launch_data_new_opaque(uuid, sizeof(uuid));
				launch_data_dict_insert(v, uuid_d, LAUNCH_JOBKEY_SECURITYSESSIONUUID);
				jobs_that_need_sessions++;
			} else {
				jobs_that_need_sessions += uuid_data_is_null(existing_v) ? 0 : 1;
			}
		}
	}
#endif

	pthread_mutex_lock(&globals->lc_mtx);

	pd_liblaunch_phase("before launchd_msg_send d=%p", d);
	if (d && launchd_msg_send(globals->l, d) == -1) {
		pd_liblaunch_phase("launchd_msg_send failed errno=%d", errno);
		do {
			if (errno != EAGAIN)
				goto out;
		} while (launchd_msg_send(globals->l, NULL) == -1);
	}
	pd_liblaunch_phase("after launchd_msg_send");

	while (resp == NULL) {
		pd_liblaunch_phase("before launchd_msg_recv");
		if (d == NULL && launch_data_array_get_count(globals->async_resp) > 0) {
			resp = launch_data_array_pop_first(globals->async_resp);
			goto out;
		}
		if (launchd_msg_recv(globals->l, launch_msg_getmsgs, &resp) == -1) {
			if (errno != EAGAIN) {
				pd_liblaunch_phase("launchd_msg_recv failed errno=%d", errno);
				goto out;
			} else if (d == NULL) {
				errno = 0;
				goto out;
			} else {
				fd_set rfds;

				FD_ZERO(&rfds);
				FD_SET(fd2use, &rfds);

				select(fd2use + 1, &rfds, NULL, NULL, NULL);
			}
		}
	}
	pd_liblaunch_phase("after launchd_msg_recv resp=%p", resp);

out:
#if !TARGET_OS_EMBEDDED
	if (!uuid_is_null(uuid) && resp && jobs_that_need_sessions > 0) {
		mach_port_t session_port = _audit_session_self();
		launch_data_type_t resp_type = launch_data_get_type(resp);

		bool set_session = false;
		if (resp_type == LAUNCH_DATA_ERRNO) {
			set_session = (launch_data_get_errno(resp) == ENEEDAUTH);
		} else if (resp_type == LAUNCH_DATA_ARRAY) {
			set_session = true;
		}

		kern_return_t kr = KERN_FAILURE;
		if (set_session) {
			kr = vproc_mig_set_security_session(bootstrap_port, uuid, session_port);
		}

		if (kr == KERN_SUCCESS) {
			if (resp_type == LAUNCH_DATA_ERRNO) {
				launch_data_set_errno(resp, 0);
			} else {
				size_t i = 0;
				for (i = 0; i < launch_data_array_get_count(resp); i++) {
					launch_data_t ri = launch_data_array_get_index(resp, i);

					int recvd_err = 0;
					if (launch_data_get_type(ri) == LAUNCH_DATA_ERRNO && (recvd_err = launch_data_get_errno(ri))) {
						launch_data_set_errno(ri, recvd_err == ENEEDAUTH ? 0 : recvd_err);
					}
				}
			}
		}

		mach_port_deallocate(mach_task_self(), session_port);
	}
#endif

	pthread_mutex_unlock(&globals->lc_mtx);

	return resp;
}

int
launchd_msg_recv(launch_t lh, void (*cb)(launch_data_t, void *), void *context)
{
	struct cmsghdr *cm = alloca(4096); 
	launch_data_t rmsg = NULL;
	size_t data_offset, fd_offset;
	struct msghdr mh;
	struct iovec iov;
	int r;

	int fd2use = launchd_getfd(lh);
	if (fd2use == -1) {
		errno = EPERM;
		return -1;
	}

	memset(&mh, 0, sizeof(mh));
	mh.msg_iov = &iov;
	mh.msg_iovlen = 1;

	lh->recvbuf = reallocf(lh->recvbuf, lh->recvlen + 8*1024);

	iov.iov_base = lh->recvbuf + lh->recvlen;
	iov.iov_len = 8*1024;
	mh.msg_control = cm;
	mh.msg_controllen = 4096;

	if ((r = recvmsg(fd2use, &mh, 0)) == -1)
		return -1;
	if (r == 0) {
		errno = ECONNRESET;
		return -1;
	}
	if (mh.msg_flags & MSG_CTRUNC) {
		errno = ECONNABORTED;
		return -1;
	}
	lh->recvlen += r;
	if (mh.msg_controllen > 0) {
		lh->recvfds = reallocf(lh->recvfds, lh->recvfdcnt * sizeof(int) + mh.msg_controllen - sizeof(struct cmsghdr));
		memcpy(lh->recvfds + lh->recvfdcnt, CMSG_DATA(cm), mh.msg_controllen - sizeof(struct cmsghdr));
		lh->recvfdcnt += (mh.msg_controllen - sizeof(struct cmsghdr)) / sizeof(int);
	}

	r = 0;

	while (lh->recvlen > 0) {
		struct launch_msg_header *lmhp = lh->recvbuf;
		uint64_t tmplen;
		data_offset = sizeof(struct launch_msg_header);
		fd_offset = 0;

		if (lh->recvlen < sizeof(struct launch_msg_header))
			goto need_more_data;

		tmplen = big2wire(lmhp->len);

		if (big2wire(lmhp->magic) != LAUNCH_MSG_HEADER_MAGIC || tmplen <= sizeof(struct launch_msg_header)) {
			errno = EBADRPC;
			goto out_bad;
		}

		if (lh->recvlen < tmplen) {
			goto need_more_data;
		}

		if ((rmsg = launch_data_unpack(lh->recvbuf, lh->recvlen, lh->recvfds, lh->recvfdcnt, &data_offset, &fd_offset)) == NULL) {
			errno = EBADRPC;
			goto out_bad;
		}

		launch_globals_t globals = _launch_globals();

		globals->in_flight_msg_recv_client = lh;

		cb(rmsg, context);

		/* launchd and only launchd can call launchd_close() as a part of the callback */
		if (globals->in_flight_msg_recv_client == NULL) {
			r = 0;
			break;
		}

		lh->recvlen -= data_offset;
		if (lh->recvlen > 0) {
			memmove(lh->recvbuf, lh->recvbuf + data_offset, lh->recvlen);
		} else {
			free(lh->recvbuf);
			lh->recvbuf = malloc(0);
		}

		lh->recvfdcnt -= fd_offset;
		if (lh->recvfdcnt > 0) {
			memmove(lh->recvfds, lh->recvfds + fd_offset, lh->recvfdcnt * sizeof(int));
		} else {
			free(lh->recvfds);
			lh->recvfds = malloc(0);
		}
	}

	return r;

need_more_data:
	errno = EAGAIN;
out_bad:
	return -1;
}

launch_data_t
launch_data_copy(launch_data_t o)
{
	return xpc_retain(o);
}

int
_fd(int fd)
{
	if (fd >= 0)
		fcntl(fd, F_SETFD, 1);
	return fd;
}

launch_data_t
launch_data_new_errno(int e)
{
	launch_data_t r = launch_data_alloc(LAUNCH_DATA_ERRNO);

	if (r)
		launch_data_set_errno(r, e);

	return r;
}

launch_data_t
launch_data_new_fd(int fd)
{
	launch_data_t r = launch_data_alloc(LAUNCH_DATA_FD);

	if (r)
		launch_data_set_fd(r, fd);

	return r;
}

launch_data_t
launch_data_new_machport(mach_port_t p)
{
	launch_data_t r = launch_data_alloc(LAUNCH_DATA_MACHPORT);

	if (r)
		launch_data_set_machport(r, p);

	return r;
}

launch_data_t
launch_data_new_integer(long long n)
{
	launch_data_t r = launch_data_alloc(LAUNCH_DATA_INTEGER);

	if (r)
		launch_data_set_integer(r, n);

	return r;
}

launch_data_t
launch_data_new_bool(bool b)
{
	launch_data_t r = launch_data_alloc(LAUNCH_DATA_BOOL);

	if (r)
		launch_data_set_bool(r, b);

	return r;
}

launch_data_t
launch_data_new_real(double d)
{
	launch_data_t r = launch_data_alloc(LAUNCH_DATA_REAL);

	if (r)
		launch_data_set_real(r, d);

	return r;
}

launch_data_t
launch_data_new_string(const char *s)
{
	launch_data_t r = launch_data_alloc(LAUNCH_DATA_STRING);

	if (r == NULL)
		return NULL;

	if (!launch_data_set_string(r, s)) {
		launch_data_free(r);
		return NULL;
	}

	return r;
}

launch_data_t
launch_data_new_opaque(const void *o, size_t os)
{
	launch_data_t r = launch_data_alloc(LAUNCH_DATA_OPAQUE);

	if (r == NULL)
		return NULL;

	if (!launch_data_set_opaque(r, o, os)) {
		launch_data_free(r);
		return NULL;
	}

	return r;
}

void
load_launchd_jobs_at_loginwindow_prompt(int flags __attribute__((unused)), ...)
{
	_vprocmgr_init(VPROCMGR_SESSION_LOGINWINDOW);
}

pid_t
create_and_switch_to_per_session_launchd(const char *login __attribute__((unused)), int flags, ...)
{
	uid_t target_user = geteuid() ? geteuid() : getuid();
	if (_vprocmgr_move_subset_to_user(target_user, VPROCMGR_SESSION_AQUA, flags)) {
		return -1;
	}

	return 1;
}

#pragma clang diagnostic pop
