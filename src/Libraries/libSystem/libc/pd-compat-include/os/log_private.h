/*
 * Minimal PureDarwin os/log_private.h compatibility shim.
 *
 * Libc's os_crash() formatting path only needs the packed-log type and helpers
 * at build time. Full libtrace/os_log is not present in this tree yet, so make a
 * compact self-contained pack that preserves the format string for crash text.
 */
#ifndef _PUREDARWIN_OS_LOG_PRIVATE_H_
#define _PUREDARWIN_OS_LOG_PRIVATE_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#if __has_include(<os/log.h>)
#include <os/log.h>
#define PD_HAVE_REAL_OS_LOG 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PD_HAVE_REAL_OS_LOG
typedef void *os_log_t;
typedef uint8_t os_log_type_t;

#ifndef OS_LOG_TYPE_DEFAULT
#define OS_LOG_TYPE_DEFAULT ((os_log_type_t)0x00)
#endif
#ifndef OS_LOG_TYPE_ERROR
#define OS_LOG_TYPE_ERROR   ((os_log_type_t)0x10)
#endif
#endif /* !PD_HAVE_REAL_OS_LOG */

/*
 * libsystem_trace now ships the real Apple-shaped pack (olp_* fields), and it
 * arrives via <os/log.h> above. Only define the compact stand-in when it did
 * not, or the two definitions collide.
 */
#ifdef PD_HAVE_REAL_OS_LOG_PACK

/* assumes.h spells the type both ways; the real header only typedefs the
 * pointer form. */
typedef struct os_log_pack_s os_log_pack_s;

size_t os_log_pack_size(const char *format, ...);
uint8_t *os_log_pack_fill(void *pack, size_t size, int saved_errno,
    const char *format, ...);

#else

typedef struct os_log_pack_s {
	uint32_t size;
	int32_t  saved_errno;
	const char *format;
} os_log_pack_s, *os_log_pack_t;

static inline size_t
os_log_pack_size(const char *format, ...)
{
	(void)format;
	return sizeof(os_log_pack_s);
}

static inline void
os_log_pack_fill(os_log_pack_t pack, size_t size, int saved_errno,
    const char *format, ...)
{
	if (!pack) return;
	pack->size = (uint32_t)size;
	pack->saved_errno = saved_errno;
	pack->format = format;
}

#endif /* PD_HAVE_REAL_OS_LOG_PACK */

#ifdef __cplusplus
}
#endif

#endif /* _PUREDARWIN_OS_LOG_PRIVATE_H_ */
