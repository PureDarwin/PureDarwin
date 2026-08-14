/*
 * <os/log_private.h> is an Apple-internal header that ships in no SDK. Security
 * only ever uses os_log_t, OS_LOG_DISABLED and os_log_create from it, all of
 * which are public <os/log.h> API, so forwarding is enough.
 */
#ifndef PD_OS_LOG_PRIVATE_H
#define PD_OS_LOG_PRIVATE_H

#include <os/log.h>

#endif /* PD_OS_LOG_PRIVATE_H */
