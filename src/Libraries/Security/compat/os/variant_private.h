/*
 * <os/variant_private.h> ships in no SDK. It reports which build variant the OS
 * is; PureDarwin has only one, a customer-style install with no internal
 * diagnostics and no recovery mode, so every query answers false.
 */
#ifndef PD_OS_VARIANT_PRIVATE_H
#define PD_OS_VARIANT_PRIVATE_H

#include <stdbool.h>

static inline bool os_variant_has_internal_content(const char *s) { (void)s; return false; }
static inline bool os_variant_has_internal_diagnostics(const char *s) { (void)s; return false; }
static inline bool os_variant_has_internal_ui(const char *s) { (void)s; return false; }
static inline bool os_variant_allows_internal_security_policies(const char *s) { (void)s; return false; }
static inline bool os_variant_is_darwinos(const char *s) { (void)s; return true; }
static inline bool os_variant_is_recovery(const char *s) { (void)s; return false; }
static inline bool os_variant_uses_ephemeral_storage(const char *s) { (void)s; return false; }

#endif /* PD_OS_VARIANT_PRIVATE_H */
