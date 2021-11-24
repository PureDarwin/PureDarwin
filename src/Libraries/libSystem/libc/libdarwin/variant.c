/*
 * Copyright (c) 2016 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <dispatch/dispatch.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#include <System/sys/csr.h>
#include <System/machine/cpu_capabilities.h>

#include <os/assumes.h>
#include <os/bsd.h>
#include <os/stdlib.h>
#include <os/variant_private.h>
#include <os/boot_mode_private.h>

/*
 * Lists all properties overridden by an empty file
 */
#define ALL_OVERRIDES_STR "content,diagnostics,ui,security"

enum variant_property {
	VP_CONTENT,
	VP_DIAGNOSTICS,
	VP_UI,
	VP_SECURITY,
	VP_MAX
};

enum check_status {
	S_UNKNOWN = 0,
	S_NO = 2,
	S_YES = 3
};

typedef struct {
	const char	*variant;
	bool		(*function)(const char*);
} variant_check_mapping;

static bool
status2bool(enum check_status status) {
	switch (status) {
	case S_NO:
		return false;
	case S_YES:
		return true;
	case S_UNKNOWN:
	default:
		os_crash("os_variant had unexpected status");
	}
}

#define VAR_FILE_LEGACY "/var/db/disableAppleInternal"

#if TARGET_OS_OSX
#define VAR_FILE_OVERRIDE "/var/db/os_variant_override"
#else
#define VAR_FILE_OVERRIDE "/usr/share/misc/os_variant_override"
#endif

#if !TARGET_OS_SIMULATOR
#define INTERNAL_CONTENT_PATH "/System/Library/CoreServices/AppleInternalVariant.plist"
#else
#define INTERNAL_CONTENT_PATH "/AppleInternal"
#endif

#define SYSTEM_VERSION_PLIST_PATH "/System/Library/CoreServices/SystemVersion.plist"
#define SYSTEM_VERSION_PLIST_KEY "ReleaseType"

#if TARGET_OS_IPHONE
#define INTERNAL_SETTINGS_PATH "/AppleInternal/Library/PreferenceBundles/Internal Settings.bundle"
#else
#define INTERNAL_DIAGS_PROFILE_PATH "/var/db/ConfigurationProfiles/Settings/com.apple.InternalDiagnostics.plist"
#define FACTORY_CONTENT_PATH "/System/Library/CoreServices/AppleFactoryVariant.plist"
#define BASE_SYSTEM_CONTENT_PATH "/System/Library/BaseSystem"
#define DARWINOS_CONTENT_PATH "/System/Library/CoreServices/DarwinVariant.plist"
#endif

static void _check_all_statuses(void);

#if !TARGET_OS_SIMULATOR
#define CACHE_SYSCTL_NAME "kern.osvariant_status"

static void _restore_cached_check_status(uint64_t status);
static uint64_t _get_cached_check_status(void);

static char * _read_file(const char *path, size_t *size_out)
{
	char *buf = NULL;

	int fd = open(path, O_RDONLY);
	if (fd == -1) return NULL;

	struct stat sb;
	int rc = fstat(fd, &sb);
	if (rc != 0 || sb.st_size == 0) {
		goto error;
	}

	size_t size_limit = (size_out && *size_out != 0) ? *size_out : 1024;
	size_t size = (size_t)sb.st_size;
	if (size_out) *size_out = (size_t)sb.st_size;
	if (size > size_limit) {
		goto error;
	}

	buf = malloc(size + 1);
	if (!buf) {
		goto error;
	}

	ssize_t bytes_read = read(fd, buf, size);
	buf[size] = '\0';


	if (bytes_read == (ssize_t)size) {
		close(fd);
		return buf;
	}

error:
	close(fd);
	free(buf);
	return NULL;
}

static xpc_object_t read_plist(const char *path)
{
	size_t size = 16 * 1024;
	uint8_t *buf = (uint8_t*)_read_file(path, &size);
	if (!buf) return NULL;

	xpc_object_t plist = xpc_create_from_plist(buf, size);
	if (plist && xpc_get_type(plist) != XPC_TYPE_DICTIONARY) {
		xpc_release(plist);
		plist = NULL;
	}

	free(buf);

	return plist;
}
#endif

#if !TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
static enum check_status internal_content = S_UNKNOWN;
#endif
#if !TARGET_OS_SIMULATOR
static enum check_status can_has_debugger = S_UNKNOWN;
static enum check_status has_full_logging = S_UNKNOWN;
#if TARGET_OS_IPHONE
static enum check_status internal_release_type = S_UNKNOWN;
static enum check_status factory_release_type = S_UNKNOWN;
static enum check_status darwin_release_type = S_UNKNOWN;
static enum check_status recovery_release_type = S_UNKNOWN;
static enum check_status development_kernel = S_UNKNOWN;
#else // TARGET_OS_IPHONE
static enum check_status internal_diags_profile = S_UNKNOWN;
static enum check_status factory_content = S_UNKNOWN;
static enum check_status base_system_content = S_UNKNOWN;
static enum check_status darwinos_content = S_UNKNOWN;
#endif // TARGET_OS_IPHONE
#endif // !TARGET_OS_SIMULATOR
static enum check_status is_ephemeral = S_UNKNOWN;

static bool disabled_status[VP_MAX] = {};

static void _parse_disabled_status(char *test_string)
{
#if TARGET_OS_SIMULATOR
#pragma unused(test_string)
#else // TARGET_OS_SIMULATOR
	char *override_str = NULL;

	bzero(disabled_status, sizeof(disabled_status));

	if (test_string != NULL) {
		/* used for unit tests */
		override_str = os_strdup(test_string);
	} else {
		if (access(VAR_FILE_LEGACY, F_OK) == 0) {
			override_str = os_strdup(ALL_OVERRIDES_STR);
		} else if (access(VAR_FILE_OVERRIDE, F_OK) != 0) {
			return;
		}

		override_str = _read_file(VAR_FILE_OVERRIDE, NULL);
	}

	if (override_str == NULL) {
		override_str = os_strdup(ALL_OVERRIDES_STR);
	}

	char *token, *string = override_str;
	while ((token = strsep(&string, ",\n")) != NULL) {
		if (strcmp(token, "content") == 0) {
			disabled_status[VP_CONTENT] = true;
		} else if (strcmp(token, "diagnostics") == 0) {
			disabled_status[VP_DIAGNOSTICS] = true;
		} else if (strcmp(token, "ui") == 0) {
			disabled_status[VP_UI] = true;
		} else if (strcmp(token, "security") == 0) {
			disabled_status[VP_SECURITY] = true;
		}
	}

	free(override_str);
	return;
#endif //!TARGET_OS_SIMULATOR
}

#if !TARGET_OS_SIMULATOR
static bool _load_cached_status(void)
{
	uint64_t status = 0;
	size_t status_size = sizeof(status);
	int ret = sysctlbyname(CACHE_SYSCTL_NAME, &status, &status_size, NULL, 0);
	if (ret != 0) {
		return false;
	}

	if (status) {
		_restore_cached_check_status(status);
		return true;
	}

	return false;
}
#endif

static void _initialize_status(void)
{
	static dispatch_once_t once;
	dispatch_once(&once, ^{
#if !TARGET_OS_SIMULATOR && !defined(VARIANT_SKIP_EXPORTED)
		if (_load_cached_status() && !_os_xbs_chrooted) {
			return;
		}
#endif
		_check_all_statuses();
	});
}

static bool _check_disabled(enum variant_property variant_property)
{
	_initialize_status();

	return disabled_status[variant_property];
}

#if !TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
static void _check_internal_content_impl(void)
{
	if (_os_xbs_chrooted && internal_content != S_UNKNOWN) {
		return;
	} else {
		os_assert(internal_content == S_UNKNOWN);
	}

#if !TARGET_OS_SIMULATOR
	const char * path = INTERNAL_CONTENT_PATH;
#else
	char *simulator_root = getenv("IPHONE_SIMULATOR_ROOT");
	char *to_free = NULL, *path = NULL;
	if (simulator_root) {
		asprintf(&path, "%s/%s", simulator_root, INTERNAL_CONTENT_PATH);
		if (path == NULL) {
			internal_content = S_NO;
			return;
		}
		to_free = path;
	}
#endif
	internal_content = (access(path, F_OK) == 0) ? S_YES : S_NO;
#if TARGET_OS_SIMULATOR
	free(to_free);
#endif
}

static bool _check_internal_content(void)
{
	_initialize_status();
	return status2bool(internal_content);
}
#endif // !TARGET_OS_IPHONE || TARGET_OS_SIMULATOR

#if TARGET_OS_OSX
static void _check_factory_content_impl(void)
{
	if (_os_xbs_chrooted && factory_content != S_UNKNOWN) {
		return;
	} else {
		os_assert(factory_content == S_UNKNOWN);
	}

	const char * path = FACTORY_CONTENT_PATH;
	factory_content = (access(path, F_OK) == 0) ? S_YES : S_NO;
}

static bool _check_factory_content(void)
{
	_initialize_status();

	return status2bool(factory_content);
}
#endif // TARGET_OS_OSX

#if TARGET_OS_IPHONE

#if !TARGET_OS_SIMULATOR
static bool _parse_system_version_plist(void)
{
	xpc_object_t system_version_plist = read_plist(SYSTEM_VERSION_PLIST_PATH);
	if (!system_version_plist) {
		return false;
	}

	const char *release_type =
			xpc_dictionary_get_string(system_version_plist,
					SYSTEM_VERSION_PLIST_KEY);

	if (release_type == NULL) {
		/*
		 * Confusingly, customer images are just completely missing this key.
		 */
		internal_release_type = S_NO;
		factory_release_type = S_NO;
		darwin_release_type = S_NO;
		recovery_release_type = S_NO;
	} else if (strcmp(release_type, "NonUI") == 0) {
		factory_release_type = S_YES;
		internal_release_type = S_YES;
		darwin_release_type = S_NO;
		recovery_release_type = S_NO;
	} else {
		factory_release_type = S_NO;
		internal_release_type = (strstr(release_type, "Internal") != NULL) ? S_YES : S_NO;
		darwin_release_type = (strstr(release_type, "Darwin") != NULL) ? S_YES : S_NO;
		recovery_release_type = (strstr(release_type, "Recovery") != NULL) ? S_YES : S_NO;
	}

	xpc_release(system_version_plist);

	return true;
}

static void _check_system_version_plist_statuses_impl(void)
{
	os_assert(internal_release_type == S_UNKNOWN);
	os_assert(factory_release_type == S_UNKNOWN);
	os_assert(darwin_release_type == S_UNKNOWN);
	os_assert(recovery_release_type == S_UNKNOWN);

	if (!_parse_system_version_plist()) {
		internal_release_type = (access(INTERNAL_SETTINGS_PATH, F_OK) == 0) ? S_YES : S_NO;
		factory_release_type = S_NO;
		darwin_release_type = S_NO;
		recovery_release_type = S_NO;
	}
}
#endif //!TARGET_OS_SIMULATOR

static bool _check_internal_release_type(void)
{
#if TARGET_OS_SIMULATOR
	return _check_internal_content();
#else // TARGET_OS_SIMULATOR
	_initialize_status();

	return status2bool(internal_release_type);
#endif // TARGET_OS_SIMULATOR
}

static bool _check_factory_release_type(void)
{
#if TARGET_OS_SIMULATOR
	return false;
#else // TARGET_OS_SIMULATOR
	_initialize_status();

	return status2bool(factory_release_type);
#endif // TARGET_OS_SIMULATOR
}

static bool _check_darwin_release_type(void)
{
#if TARGET_OS_SIMULATOR
	return false;
#else // TARGET_OS_SIMULATOR
	_initialize_status();

	return status2bool(darwin_release_type);
#endif // TARGET_OS_SIMULATOR
}

static bool _check_recovery_release_type(void)
{
#if TARGET_OS_SIMULATOR
	return false;
#else // TARGET_OS_SIMULATOR
	_initialize_status();

	return status2bool(recovery_release_type);
#endif // TARGET_OS_SIMULATOR
}

#else // TARGET_OS_IPHONE

static void _check_internal_diags_profile_impl(void)
{
	if (_os_xbs_chrooted && internal_diags_profile != S_UNKNOWN) {
		return;
	} else {
		os_assert(internal_diags_profile == S_UNKNOWN);
	}

	xpc_object_t profile_settings = read_plist(INTERNAL_DIAGS_PROFILE_PATH);
	if (profile_settings) {
		internal_diags_profile = xpc_dictionary_get_bool(profile_settings, "AppleInternal") ? S_YES : S_NO;
		xpc_release(profile_settings);
	} else {
		internal_diags_profile = S_NO;
	}
}

static bool _check_internal_diags_profile(void)
{
	_initialize_status();

	return status2bool(internal_diags_profile);
}

static void _check_base_system_content_impl(void)
{
	if (_os_xbs_chrooted && base_system_content != S_UNKNOWN) {
		return;
	} else {
		os_assert(base_system_content == S_UNKNOWN);
	}

	const char * path = BASE_SYSTEM_CONTENT_PATH;
	base_system_content = (access(path, F_OK) == 0) ? S_YES : S_NO;
}

static bool _check_base_system_content(void)
{
	_initialize_status();

	return status2bool(base_system_content);
}

static void _check_darwinos_content_impl(void)
{
	if (_os_xbs_chrooted && darwinos_content != S_UNKNOWN) {
		return;
	} else {
		os_assert(darwinos_content == S_UNKNOWN);
	}

	const char * path = DARWINOS_CONTENT_PATH;
	darwinos_content = (access(path, F_OK) == 0) ? S_YES : S_NO;
}

static bool _check_darwinos_content(void)
{
	_initialize_status();

	return status2bool(darwinos_content);
}

#endif

#if !TARGET_OS_SIMULATOR
static void _check_can_has_debugger_impl(void)
{
	if (_os_xbs_chrooted && can_has_debugger != S_UNKNOWN) {
		return;
	} else {
		os_assert(can_has_debugger == S_UNKNOWN);
	}

#if TARGET_OS_IPHONE
	can_has_debugger = *((uint32_t *)_COMM_PAGE_DEV_FIRM) ? S_YES : S_NO;
#else
	/*
	 * The comm page bit does exist on macOS, but also requires kernel
	 * debugging in the CSR configuration.  We don't need to be that strict
	 * here.
	 */
	can_has_debugger = (csr_check(CSR_ALLOW_APPLE_INTERNAL) == 0) ? S_YES : S_NO;
#endif
}

static bool _check_can_has_debugger(void)
{
	_initialize_status();

	return status2bool(can_has_debugger);
}
#endif // !TARGET_OS_SIMULATOR

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
static void _check_development_kernel_impl(void)
{
	os_assert(development_kernel == S_UNKNOWN);
	/*
	* Whitelist values from SUPPORTED_KERNEL_CONFIGS.
	 */
	char *osbuildconfig = NULL;
	size_t osbuildconfig_sz = 0;
	errno_t err = sysctlbyname_get_data_np("kern.osbuildconfig", (void **)&osbuildconfig, &osbuildconfig_sz);
	if (err == 0) {
		if (strcmp(osbuildconfig, "development") == 0 ||
				strcmp(osbuildconfig, "debug") == 0 ||
				strcmp(osbuildconfig, "profile") == 0 ||
				strcmp(osbuildconfig, "kasan") == 0) {
			development_kernel = S_YES;
		}
	}
	free(osbuildconfig);

	if (development_kernel == S_UNKNOWN) {
		development_kernel = S_NO;
	}
}

static bool _check_development_kernel(void)
{
	_initialize_status();

	return status2bool(development_kernel);
}
#endif // TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR

static void _check_uses_ephemeral_storage_impl(void)
{
	if (_os_xbs_chrooted && is_ephemeral != S_UNKNOWN) {
		return;
	} else {
		os_assert(is_ephemeral == S_UNKNOWN);
	}

	uint32_t buffer = 0;
	size_t buffer_size = sizeof(buffer);

	sysctlbyname("hw.ephemeral_storage", (void *)&buffer, &buffer_size, NULL, 0);

	is_ephemeral = (buffer != 0) ? S_YES : S_NO;
}

static bool _check_uses_ephemeral_storage(void)
{
	_initialize_status();

	return status2bool(is_ephemeral);
}

#if !TARGET_OS_SIMULATOR
// internal upcall into libtrace
extern bool
_os_trace_basesystem_storage_available(void);

static void
_init_has_full_logging(void)
{
#if TARGET_OS_OSX
	if (_check_base_system_content() &&
			!_os_trace_basesystem_storage_available()) {
		has_full_logging = S_NO;
		return;
	}
#endif

	has_full_logging = S_YES;
}

static bool _check_has_full_logging(void)
{
	_initialize_status();

	return status2bool(has_full_logging);
}
#endif // !TARGET_OS_SIMULATOR

static void _check_all_statuses(void)
{
#if !TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
	_check_internal_content_impl();
#endif

	_check_uses_ephemeral_storage_impl();

#if !TARGET_OS_SIMULATOR
	_check_can_has_debugger_impl();

#if TARGET_OS_IPHONE
	_check_system_version_plist_statuses_impl();
	_check_development_kernel_impl();
#else
	_check_internal_diags_profile_impl();
	_check_factory_content_impl();
	_check_base_system_content_impl();
	_check_darwinos_content_impl();
#endif

#endif // !TARGET_OS_SIMULUATOR

	_parse_disabled_status(NULL);
}

static bool
os_variant_has_full_logging(const char * __unused subsystem)
{
#if TARGET_OS_SIMULATOR
	return true;
#else
	return _check_has_full_logging();
#endif
}

static const variant_check_mapping _variant_map[] = {
	{.variant = "AllowsInternalSecurityPolicies", .function = os_variant_allows_internal_security_policies},
	{.variant = "HasFactoryContent", .function = os_variant_has_factory_content},
	{.variant = "HasFullLogging", .function = os_variant_has_full_logging},
	{.variant = "HasInternalContent", .function = os_variant_has_internal_content},
	{.variant = "HasInternalDiagnostics", .function = os_variant_has_internal_diagnostics},
	{.variant = "HasInternalUI", .function = os_variant_has_internal_ui},
#if TARGET_OS_OSX
	{.variant = "IsBaseSystem", .function = os_variant_is_basesystem},
#endif
	{.variant = "IsDarwinOS", .function = os_variant_is_darwinos},
	{.variant = "IsRecovery", .function = os_variant_is_recovery},
	{.variant = "UsesEphemeralStorage", .function = os_variant_uses_ephemeral_storage},
	{.variant = NULL, .function = NULL}
};

// For unit tests
#ifndef VARIANT_SKIP_EXPORTED

bool
os_variant_has_internal_content(const char * __unused subsystem)
{
	if (_check_disabled(VP_CONTENT)) {
		return false;
	}

#if TARGET_OS_IPHONE
	return _check_internal_release_type();
#else
	return _check_internal_content();
#endif
}


bool
os_variant_has_internal_diagnostics(const char * __unused subsystem)
{
	if (_check_disabled(VP_DIAGNOSTICS)) {
		return false;
	}

#if TARGET_OS_IPHONE
	return _check_internal_release_type();
#else
	return _check_internal_content() || _check_internal_diags_profile();
#endif
}

bool
os_variant_has_internal_ui(const char * __unused subsystem)
{
	if (_check_disabled(VP_UI)) {
		return false;
	}

#if TARGET_OS_IPHONE
	return _check_internal_release_type();
#else
	return _check_internal_content();
#endif
}

bool
os_variant_allows_internal_security_policies(const char * __unused subsystem)
{
	if (_check_disabled(VP_SECURITY)) {
		return false;
	}

#if TARGET_OS_SIMULATOR
	return _check_internal_content();
#elif TARGET_OS_IPHONE
	return _check_can_has_debugger() || _check_development_kernel();
#else
	return _check_can_has_debugger();
#endif
}

bool
os_variant_has_factory_content(const char * __unused subsystem)
{
#if TARGET_OS_IPHONE
	return _check_factory_release_type();
#else
	return _check_factory_content();
#endif
}

bool
os_variant_is_darwinos(const char * __unused subsystem)
{
#if TARGET_OS_IPHONE
	return _check_darwin_release_type();
#else
	return _check_darwinos_content();
#endif
}

bool
os_variant_is_recovery(const char * __unused subsystem)
{
#if TARGET_OS_IPHONE
	return _check_recovery_release_type();
#else
	return _check_base_system_content();
#endif
}

#if TARGET_OS_OSX
bool
os_variant_is_basesystem(const char * __unused subsystem)
{
	return _check_base_system_content();
}
#endif

bool
os_variant_uses_ephemeral_storage(const char * __unused subsystem)
{
	return _check_uses_ephemeral_storage();
}

bool
os_variant_check(const char *subsystem, const char *variant)
{
	variant_check_mapping *current = (variant_check_mapping *)_variant_map;

	while (current->variant) {
		if (0 == strncasecmp(current->variant, variant, strlen(current->variant))) {
			return current->function(subsystem);
		}
		current ++;
	}

	return false;
}

char *
os_variant_copy_description(const char *subsystem)
{
	variant_check_mapping *current = (variant_check_mapping *)_variant_map;

	char *desc = NULL;
	size_t desc_size = 0;
	FILE *outstream = open_memstream(&desc, &desc_size);
	if (!outstream) {
		return NULL;
	}

	int error = 0;
	bool needs_space = false;
	while (current->variant) {
		if (current->function(subsystem)) {
			if (needs_space) {
				int written = fputc(' ', outstream);
				if (written == EOF) {
					error = errno;
					goto error_out;
				}
			}
			int written = fputs(current->variant, outstream);
			if (written == EOF) {
				error = errno;
				goto error_out;
			}
			needs_space = true;
		}
		current++;
	}

	int closed = fclose(outstream);
	if (closed == EOF) {
		error = errno;
		goto close_error_out;
	}
	return desc;

error_out:
	(void)fclose(outstream);
close_error_out:
	free(desc);
	errno = error;
	return NULL;
}

#if TARGET_OS_OSX

// XXX As an implementation detail, os_boot_mode is piggy-backing on
// os_variant's infrastructure.  This is not necessarily its long-term home,
// particularly after rdar://59966472

static enum boot_mode {
	BOOTMODE_UNKNOWN = 0,
	BOOTMODE_NONE,
	BOOTMODE_FVUNLOCK,
	BOOTMODE_KCGEN,
	BOOTMODE_DIAGNOSTICS,
	BOOTMODE_MIGRATION,
} os_boot_mode;

static void
_os_boot_mode_launchd_init(const char *boot_mode)
{
	if (boot_mode == NULL) {
		os_boot_mode = BOOTMODE_NONE;
	} else if (strcmp(boot_mode, OS_BOOT_MODE_FVUNLOCK) == 0) {
		os_boot_mode = BOOTMODE_FVUNLOCK;
	} else if (strcmp(boot_mode, OS_BOOT_MODE_KCGEN) == 0) {
		os_boot_mode = BOOTMODE_KCGEN;
	} else if (strcmp(boot_mode, OS_BOOT_MODE_DIAGNOSTICS) == 0) {
		os_boot_mode = BOOTMODE_DIAGNOSTICS;
	} else if (strcmp(boot_mode, OS_BOOT_MODE_MIGRATION) == 0) {
		os_boot_mode = BOOTMODE_MIGRATION;
	}
}

bool
os_boot_mode_query(const char **boot_mode_out)
{
	_initialize_status();

	switch (os_boot_mode) {
	case BOOTMODE_NONE:
		*boot_mode_out = NULL;
		return true;
	case BOOTMODE_FVUNLOCK:
		*boot_mode_out = OS_BOOT_MODE_FVUNLOCK;
		return true;
	case BOOTMODE_KCGEN:
		*boot_mode_out = OS_BOOT_MODE_KCGEN;
		return true;
	case BOOTMODE_DIAGNOSTICS:
		*boot_mode_out = OS_BOOT_MODE_DIAGNOSTICS;
		return true;
	case BOOTMODE_MIGRATION:
		*boot_mode_out = OS_BOOT_MODE_MIGRATION;
		return true;
	default:
		return false;
	}
}

#endif // TARGET_OS_OSX

void
os_variant_init_4launchd(const char *boot_mode)
{
#if TARGET_OS_SIMULATOR
	os_crash("simulator launchd does not initialize os_variant");
#else
	os_assert(getpid() == 1);

	_init_has_full_logging();

#if TARGET_OS_OSX
	_os_boot_mode_launchd_init(boot_mode);
#endif

	// re-initialize disabled status even if we've already initialized
	// previously, as it's possible we may have initialized before the override
	// file was available to read
	_parse_disabled_status(NULL);

	uint64_t status = _get_cached_check_status();
	size_t status_size = sizeof(status);
	// TODO: assert that this succeeds
	sysctlbyname(CACHE_SYSCTL_NAME, NULL, 0, &status, status_size);
#endif
}

#endif // VARIANT_SKIP_EXPORTED

/*
 * Bit allocation in kern.osvariant_status (all ranges inclusive):
 * - [0-27] are 2-bit check_status values
 * - [28-31] are 0xF
 * - [32-32+VP_MAX-1] encode variant_property booleans
 * - [48-51] encode the boot mode, if known
 * - [60-62] are 0x7
 */
#define STATUS_INITIAL_BITS 0x70000000F0000000ULL
#define STATUS_BIT_WIDTH 2
#define STATUS_SET 0x2
#define STATUS_MASK 0x3

enum status_flags_positions {
	SFP_INTERNAL_CONTENT = 0,
	SFP_CAN_HAS_DEBUGGER = 1,
	SFP_INTERNAL_RELEASE_TYPE = 2,
	SFP_INTERNAL_DIAGS_PROFILE = 3,
	SFP_FACTORY_CONTENT = 4,
	SFP_FACTORY_RELEASE_TYPE = 5,
	SFP_DARWINOS_RELEASE_TYPE = 6,
	SFP_EPHEMERAL_VOLUME = 7,
	SFP_RECOVERY_RELEASE_TYPE = 8,
	SFP_BASE_SYSTEM_CONTENT = 9,
	SFP_DEVELOPMENT_KERNEL = 10,
	SFP_DARWINOS_CONTENT = 11,
	SFP_FULL_LOGGING = 12,
};

#define STATUS_BOOT_MODE_SHIFT 48
#define STATUS_BOOT_MODE_MASK 0x000F000000000000ULL

#if !TARGET_OS_SIMULATOR
static uint64_t _get_cached_check_status(void)
{
	_initialize_status();

	uint64_t res = STATUS_INITIAL_BITS;

#if !TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
	os_assert(internal_content != S_UNKNOWN);
	res |= internal_content << SFP_INTERNAL_CONTENT * STATUS_BIT_WIDTH;
#endif

	os_assert(can_has_debugger != S_UNKNOWN);
	res |= can_has_debugger << SFP_CAN_HAS_DEBUGGER * STATUS_BIT_WIDTH;

	os_assert(is_ephemeral != S_UNKNOWN);
	res |= is_ephemeral << SFP_EPHEMERAL_VOLUME * STATUS_BIT_WIDTH;

#ifdef VARIANT_SKIP_EXPORTED
	// has_full_logging can't be computed outside launchd, so in the tests/etc.
	// cheat and use the value reported by libdarwin rather than re-computing
	has_full_logging = os_variant_check("com.apple.Libc.tests", "HasFullLogging") ?
			S_YES : S_NO;
#else
	os_assert(has_full_logging != S_UNKNOWN);
#endif
	res |= has_full_logging << SFP_FULL_LOGGING * STATUS_BIT_WIDTH;

#if TARGET_OS_IPHONE
	os_assert(internal_release_type != S_UNKNOWN);
	res |= internal_release_type << SFP_INTERNAL_RELEASE_TYPE * STATUS_BIT_WIDTH;

	os_assert(factory_release_type != S_UNKNOWN);
	res |= factory_release_type << SFP_FACTORY_RELEASE_TYPE * STATUS_BIT_WIDTH;

	os_assert(darwin_release_type != S_UNKNOWN);
	res |= darwin_release_type << SFP_DARWINOS_RELEASE_TYPE * STATUS_BIT_WIDTH;

	os_assert(recovery_release_type != S_UNKNOWN);
	res |= recovery_release_type << SFP_RECOVERY_RELEASE_TYPE * STATUS_BIT_WIDTH;

	os_assert(development_kernel != S_UNKNOWN);
	res |= development_kernel << SFP_DEVELOPMENT_KERNEL * STATUS_BIT_WIDTH;
#else
	os_assert(internal_diags_profile != S_UNKNOWN);
	res |= internal_diags_profile << SFP_INTERNAL_DIAGS_PROFILE * STATUS_BIT_WIDTH;

	os_assert(factory_content != S_UNKNOWN);
	res |= factory_content << SFP_FACTORY_CONTENT * STATUS_BIT_WIDTH;

	os_assert(base_system_content != S_UNKNOWN);
	res |= base_system_content << SFP_BASE_SYSTEM_CONTENT * STATUS_BIT_WIDTH;

	os_assert(darwinos_content != S_UNKNOWN);
	res |= darwinos_content << SFP_DARWINOS_CONTENT * STATUS_BIT_WIDTH;
#endif

	for (int i = 0; i < VP_MAX; i++) {
		if (disabled_status[i]) {
			res |= 0x1ULL << (i + 32);
		}
	}

#if !defined(VARIANT_SKIP_EXPORTED) && TARGET_OS_OSX
	res |= ((uint64_t)os_boot_mode) << STATUS_BOOT_MODE_SHIFT;
#endif // TARGET_OS_OSX

	return res;
}

static void _restore_cached_check_status(uint64_t status)
{
#if !TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
	if ((status >> (SFP_INTERNAL_CONTENT * STATUS_BIT_WIDTH)) & STATUS_SET)
		internal_content = (status >> (SFP_INTERNAL_CONTENT * STATUS_BIT_WIDTH)) & STATUS_MASK;
#endif

	if ((status >> (SFP_CAN_HAS_DEBUGGER * STATUS_BIT_WIDTH)) & STATUS_SET)
		can_has_debugger = (status >> (SFP_CAN_HAS_DEBUGGER * STATUS_BIT_WIDTH)) & STATUS_MASK;

	if ((status >> (SFP_EPHEMERAL_VOLUME * STATUS_BIT_WIDTH)) & STATUS_SET)
		is_ephemeral = (status >> (SFP_EPHEMERAL_VOLUME * STATUS_BIT_WIDTH)) & STATUS_MASK;

	if ((status >> (SFP_FULL_LOGGING * STATUS_BIT_WIDTH)) & STATUS_SET)
		has_full_logging = (status >> (SFP_FULL_LOGGING * STATUS_BIT_WIDTH)) & STATUS_MASK;

#if TARGET_OS_IPHONE
	if ((status >> (SFP_INTERNAL_RELEASE_TYPE * STATUS_BIT_WIDTH)) & STATUS_SET)
		internal_release_type = (status >> (SFP_INTERNAL_RELEASE_TYPE * STATUS_BIT_WIDTH)) & STATUS_MASK;

	if ((status >> (SFP_FACTORY_RELEASE_TYPE * STATUS_BIT_WIDTH)) & STATUS_SET)
		factory_release_type = (status >> (SFP_FACTORY_RELEASE_TYPE * STATUS_BIT_WIDTH)) & STATUS_MASK;

	if ((status >> (SFP_DARWINOS_RELEASE_TYPE * STATUS_BIT_WIDTH)) & STATUS_SET)
		darwin_release_type = (status >> (SFP_DARWINOS_RELEASE_TYPE * STATUS_BIT_WIDTH)) & STATUS_MASK;

	if ((status >> (SFP_RECOVERY_RELEASE_TYPE * STATUS_BIT_WIDTH)) & STATUS_SET)
		recovery_release_type = (status >> (SFP_RECOVERY_RELEASE_TYPE * STATUS_BIT_WIDTH)) & STATUS_MASK;

	if ((status >> (SFP_DEVELOPMENT_KERNEL * STATUS_BIT_WIDTH)) & STATUS_SET)
		development_kernel = (status >> (SFP_DEVELOPMENT_KERNEL * STATUS_BIT_WIDTH)) & STATUS_MASK;
#else
	if ((status >> (SFP_INTERNAL_DIAGS_PROFILE * STATUS_BIT_WIDTH)) & STATUS_SET)
		internal_diags_profile = (status >> (SFP_INTERNAL_DIAGS_PROFILE * STATUS_BIT_WIDTH)) & STATUS_MASK;

	if ((status >> (SFP_FACTORY_CONTENT * STATUS_BIT_WIDTH)) & STATUS_SET)
		factory_content = (status >> (SFP_FACTORY_CONTENT * STATUS_BIT_WIDTH)) & STATUS_MASK;

	if ((status >> (SFP_BASE_SYSTEM_CONTENT * STATUS_BIT_WIDTH)) & STATUS_SET)
		base_system_content = (status >> (SFP_BASE_SYSTEM_CONTENT * STATUS_BIT_WIDTH)) & STATUS_MASK;

	if ((status >> (SFP_DARWINOS_CONTENT * STATUS_BIT_WIDTH)) & STATUS_SET)
		darwinos_content = (status >> (SFP_DARWINOS_CONTENT * STATUS_BIT_WIDTH)) & STATUS_MASK;
#endif

	for (int i = 0; i < VP_MAX; i++) {
		disabled_status[i] = (status >> (32 + i)) & 0x1;
	}

#if !defined(VARIANT_SKIP_EXPORTED) && TARGET_OS_OSX
	os_boot_mode = (enum boot_mode)((status & STATUS_BOOT_MODE_MASK) >> STATUS_BOOT_MODE_SHIFT);
#endif // TARGET_OS_OSX
}
#endif // !TARGET_OS_SIMULATOR
