#ifndef _PUREDARWIN_SCPRIVATE_H_
#define _PUREDARWIN_SCPRIVATE_H_

/*
 * dnsinfo_internal.h includes <SystemConfiguration/SCPrivate.h> solely to pick
 * up SC_log, and only as the fallback when my_log is not already defined. The
 * dnsinfo client (dnsinfo_copy.c) defines my_log to os_log() before including
 * it, so nothing here is ever expanded.
 *
 * The real SCPrivate.h cannot be used from this archive: it pulls in
 * CoreFoundation, which links against libSystem, and this archive is
 * force-loaded into libSystem. Only the configd side, which links CF
 * normally, gets the genuine header.
 */

#include <os/log.h>
#include <syslog.h>

#ifndef SC_log
#define SC_log(__level, __format, ...)                                        \
	os_log(OS_LOG_DEFAULT, __format, ## __VA_ARGS__)
#endif

#endif /* _PUREDARWIN_SCPRIVATE_H_ */
