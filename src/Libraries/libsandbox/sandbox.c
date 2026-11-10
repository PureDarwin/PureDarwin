/*
 * sandbox.c - sandbox_init(3): compiles a profile and hands it to Sandbox.kext.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sandbox.h>

#include "sandbox_profile.h"
#include "sbpl.h"

extern int __mac_syscall(const char *policy, int call, void *arg);

const char kSBXProfileNoInternet[] = "no-internet";
const char kSBXProfileNoNetwork[] = "no-network";
const char kSBXProfileNoWrite[] = "no-write";
const char kSBXProfileNoWriteExceptTemporary[] = "no-write-except-temporary";
const char kSBXProfilePureComputation[] = "pure-computation";

static const struct {
	const char *name;
	const char *source;
} named_profiles[] = {
	{ kSBXProfileNoInternet,
	  "(version 1) (allow default)"
	  " (deny network-outbound (remote ip)) (deny network-inbound (local ip))" },
	{ kSBXProfileNoNetwork, "(version 1) (allow default) (deny network*)" },
	{ kSBXProfileNoWrite, "(version 1) (allow default) (deny file-write*)" },
	{ kSBXProfileNoWriteExceptTemporary,
	  "(version 1) (allow default) (deny file-write*)"
	  " (allow file-write* (subpath \"/tmp\") (subpath \"/var/tmp\")"
	  " (subpath \"/private/tmp\") (subpath \"/private/var/tmp\"))" },
	{ kSBXProfilePureComputation, "(version 1) (deny default)" },
};

static void
set_error(char **errorbuf, const char *format, ...)
{
	va_list ap;

	if (errorbuf == NULL) {
		return;
	}
	va_start(ap, format);
	if (vasprintf(errorbuf, format, ap) < 0) {
		*errorbuf = NULL;
	}
	va_end(ap);
}

// a named profile outside the built-in table is <name>.sb in the profile directory
static char *
read_named_profile(const char *name)
{
	char *path = NULL, *source;

	if (name[0] == '\0' || strchr(name, '/') != NULL || strcmp(name, "..") == 0) {
		errno = ENOENT;
		return NULL;
	}
	if (asprintf(&path, "%s/%s.sb", SBPL_PROFILE_DIR, name) < 0) {
		errno = ENOMEM;
		return NULL;
	}
	source = sbpl_read_file(path);
	free(path);
	return source;
}

int
sandbox_init_with_parameters(const char *profile, uint64_t flags,
    const char *const parameters[], char **errorbuf)
{
	const char *source = profile;
	struct sb_set_profile_args args;
	char *error = NULL, *loaded = NULL;
	void *blob;
	size_t size;
	int saved;

	if (errorbuf != NULL) {
		*errorbuf = NULL;
	}
	if (profile == NULL) {
		set_error(errorbuf, "no profile given");
		errno = EINVAL;
		return -1;
	}
	if (flags & SANDBOX_NAMED) {
		source = NULL;
		for (size_t i = 0; i < sizeof(named_profiles) / sizeof(named_profiles[0]); i++) {
			if (strcmp(named_profiles[i].name, profile) == 0) {
				source = named_profiles[i].source;
			}
		}
		if (source == NULL) {
			loaded = read_named_profile(profile);
			if (loaded == NULL) {
				set_error(errorbuf, "unknown profile name '%s': %s", profile, strerror(errno));
				errno = EINVAL;
				return -1;
			}
			source = loaded;
		}
	} else if (flags != 0) {
		set_error(errorbuf, "unsupported flags 0x%llx", (unsigned long long)flags);
		errno = EINVAL;
		return -1;
	}

	if (sbpl_compile(source, parameters, NULL, &blob, &size, &error) != 0) {
		free(loaded);
		if (errorbuf != NULL) {
			*errorbuf = error;
		} else {
			free(error);
		}
		errno = EINVAL;
		return -1;
	}
	free(loaded);

	args.profile = (uint64_t)(uintptr_t)blob;
	args.size = size;
	if (__mac_syscall(SB_POLICY_NAME, SB_CALL_SET_PROFILE, &args) == 0) {
		free(blob);
		return 0;
	}
	saved = errno;
	free(blob);
	if (saved == ENOPOLICY) {
		set_error(errorbuf, "the Sandbox kernel policy is not loaded");
	} else if (saved == EPERM) {
		set_error(errorbuf, "the process is already sandboxed");
	} else {
		set_error(errorbuf, "applying the profile failed: %s", strerror(saved));
	}
	errno = saved;
	return -1;
}

int
sandbox_init(const char *profile, unsigned long flags, char **errorbuf)
{
	return sandbox_init_with_parameters(profile, flags, NULL, errorbuf);
}

void
sandbox_free_error(char *errorbuf)
{
	free(errorbuf);
}
