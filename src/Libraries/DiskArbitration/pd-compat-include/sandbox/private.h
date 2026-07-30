/*
 * <sandbox/private.h> is Apple's private sandbox SPI header, and unlike most of
 * the missing headers here it is not vestigial: DAServer.c calls
 *
 *     sandbox_check_by_audit_token(token, "file-mount",
 *                                  SANDBOX_FILTER_PATH | SANDBOX_CHECK_ALLOW_APPROVAL,
 *                                  path)
 *
 * to ask whether the client asking for a mount is permitted to mount at that
 * path. PureDarwin has no Sandbox.kext and so no MAC policy to consult; the
 * existing PureDarwin sandbox_check* (XPC/notify/pd_sandbox_check.c) report
 * "allowed", which is the documented behaviour when no profile is in effect.
 */

#ifndef _PUREDARWIN_SANDBOX_PRIVATE_H_
#define _PUREDARWIN_SANDBOX_PRIVATE_H_

#include <sandbox.h>

#endif /* _PUREDARWIN_SANDBOX_PRIVATE_H_ */
