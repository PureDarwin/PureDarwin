#ifndef _PUREDARWIN_NE_SESSION_H_
#define _PUREDARWIN_NE_SESSION_H_

#include <stdbool.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/*
 * Session status, as passed to SCNetworkConnectionGetStatusFromNEStatus().
 * Values match NetworkExtension's ne_session_status_t; nothing here produces
 * one, but the declaration has to exist for the header to parse.
 */
typedef enum {
	NESessionStatusInvalid		= 0,
	NESessionStatusDisconnected	= 1,
	NESessionStatusConnecting	= 2,
	NESessionStatusConnected	= 3,
	NESessionStatusReasserting	= 4,
	NESessionStatusDisconnecting	= 5,
} ne_session_status_t;

// needs a private KPI, just avoid it

static inline bool
ne_session_always_on_vpn_configs_present(void)
{
	return false;
}

__END_DECLS

#endif /* _PUREDARWIN_NE_SESSION_H_ */
