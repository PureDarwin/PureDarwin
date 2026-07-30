#ifndef _PUREDARWIN_SC_PRELUDE_H_
#define _PUREDARWIN_SC_PRELUDE_H_

#include <stdint.h>

#ifndef _PUREDARWIN_USER_SIZED_TYPES_
#define _PUREDARWIN_USER_SIZED_TYPES_

typedef uint64_t pd_user64_addr_t __attribute__((aligned(8)));
typedef uint32_t pd_user32_addr_t;

#define user64_addr_t  pd_user64_addr_t
#define user32_addr_t  pd_user32_addr_t

#endif /* _PUREDARWIN_USER_SIZED_TYPES_ */

#ifndef kIOUSBAppleVendorID
#define kIOUSBAppleVendorID  0x05AC
#endif

#include <stdbool.h>
extern bool dyld_process_is_restricted(void);

/*
 * Two version skews between configd-1109 and the other Apple sources vendored
 * here. Both are renames, not missing functionality.
 */
#include <si_compare.h>
#ifndef sa_dst_compare_no_dependencies
#define sa_dst_compare_no_dependencies si_destination_compare_no_dependencies
#endif

/*
 * If a VPN controller is ever ported, replace this with its real header:
 * a wrong VPN_RUNNING would silently mark a connected VPN unreachable.
 */
#ifndef VPN_IDLE
enum {
	VPN_IDLE      = 0,
	VPN_LOADING   = 1,
	VPN_LOADED    = 2,
	VPN_UNLOADING = 3,
	VPN_RUNNING   = 4,
};
#endif

#endif /* _PUREDARWIN_SC_PRELUDE_H_ */
