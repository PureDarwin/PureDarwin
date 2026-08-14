/*
 * <os/feature_private.h> backs Apple's runtime feature flags, which are read
 * from a plist their tooling generates at build time. PureDarwin ships no such
 * plist, so every feature reports its off-by-default state.
 */
#ifndef PD_OS_FEATURE_PRIVATE_H
#define PD_OS_FEATURE_PRIVATE_H

#include <stdbool.h>

#define os_feature_enabled(domain, feature) (false)
#define os_feature_enabled_simple(domain, feature, dflt) (dflt)

#endif /* PD_OS_FEATURE_PRIVATE_H */
