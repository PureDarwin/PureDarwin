/*
 * os_transaction: libdispatch SPI telling the system a process has work in
 * flight and should not be suspended or jetsammed. PureDarwin's libdispatch does
 * not implement it, and PureDarwin does not suspend or jetsam daemons, so there
 * is nothing for a transaction to hold off.
 *
 * os_transaction_create() therefore reports that no transaction was taken.
 * Callers keep the handle and later os_release() it, which is safe on NULL.
 * diskarbitrationd is the caller that matters: it wraps disk-probing work in
 * ___os_transaction_begin/end so launchd will not stop it mid-probe. With
 * nothing stopping it, the probe runs to completion either way.
 */

#ifndef _PUREDARWIN_OS_TRANSACTION_PRIVATE_H_
#define _PUREDARWIN_OS_TRANSACTION_PRIVATE_H_

#include <stddef.h>

typedef struct os_transaction_s *os_transaction_t;

static __inline__ os_transaction_t
os_transaction_create(const char *description)
{
	(void)description;
	return NULL;
}

#endif /* _PUREDARWIN_OS_TRANSACTION_PRIVATE_H_ */
