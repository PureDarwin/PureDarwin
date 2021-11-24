/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#ifndef __OS_BOOT_MODE_H__
#define __OS_BOOT_MODE_H__

#include <stdbool.h>

#include <os/base.h>
#include <os/availability.h>

__BEGIN_DECLS

#define OS_BOOT_MODE_FVUNLOCK "fvunlock"
#define OS_BOOT_MODE_KCGEN "kcgen"
#define OS_BOOT_MODE_DIAGNOSTICS "diagnostics"
#define OS_BOOT_MODE_MIGRATION "migration"

/*!
 * @function os_boot_mode_query
 *
 * @abstract fetches the current boot mode if available
 *
 * @description
 * This attempts to query the current boot mode.
 *
 * In general, please prefer the _LimitLoad[To|From]BootMode launchd plist key
 * over direct use of this SPI.
 *
 * CAVEATS:
 * - this is not guaranteed to succeed when called from boot tasks (we may not
 *   have figured out our boot mode yet)
 * - though the boot mode can in principle be an arbitrary string, this can
 *   currently only be used to query for the "fvunlock", "kcgen",  "diagnostics",
 *   and "migration" boot modes
 *
 * @result
 * true if the query succeeds, with boot_mode_out set to the boot mode string
 * (or NULL if no particular boot mode).  false if the boot mode is not known,
 * in which case boot_mode_out is not modified.
 */
API_AVAILABLE(macosx(10.16)) API_UNAVAILABLE(ios, tvos, watchos, bridgeos)
OS_EXPORT OS_WARN_RESULT
bool
os_boot_mode_query(const char **boot_mode_out);

__END_DECLS

#endif // __OS_BOOT_MODE_H__
