#include <os/variant_private.h>

#include <darwintest.h>

/*
 * Most of these are MAYFAIL because the test might sometimes run on non-internal devices.
 */

T_DECL(os_variant_basic, "Just calls all the APIs")
{
	T_MAYFAIL;
	T_EXPECT_TRUE(os_variant_has_internal_content("com.apple.Libc.tests"), NULL);

	T_MAYFAIL;
	T_EXPECT_TRUE(os_variant_has_internal_diagnostics("com.apple.Libc.tests"), NULL);

	T_MAYFAIL;
	T_EXPECT_TRUE(os_variant_has_internal_ui("com.apple.Libc.tests"), NULL);

	T_MAYFAIL;
	T_EXPECT_TRUE(os_variant_allows_internal_security_policies("com.apple.Libc.tests"), NULL);

	T_MAYFAIL;
	T_EXPECT_FALSE(os_variant_has_factory_content("com.apple.Libc.tests"), NULL);

	T_MAYFAIL;
	T_EXPECT_FALSE(os_variant_is_darwinos("com.apple.Libc.tests"), NULL);

	T_MAYFAIL;
	T_EXPECT_FALSE(os_variant_uses_ephemeral_storage("com.apple.Libc.tests"), NULL);

	T_MAYFAIL;
	T_EXPECT_TRUE(os_variant_check("com.apple.Libc.tests", "HasFullLogging"), NULL);
}

#define VARIANT_SKIP_EXPORTED

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "../libdarwin/variant.c"
#pragma clang diagnostic pop

T_DECL(os_variant_detailed, "Looks at individual checks")
{
	T_MAYFAIL;
	T_EXPECT_FALSE(_check_disabled(VP_CONTENT), NULL);

	T_MAYFAIL;
	T_EXPECT_FALSE(_check_disabled(VP_DIAGNOSTICS), NULL);

	T_MAYFAIL;
	T_EXPECT_FALSE(_check_disabled(VP_UI), NULL);

	T_MAYFAIL;
	T_EXPECT_FALSE(_check_disabled(VP_SECURITY), NULL);

#if !TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
	T_MAYFAIL;
	T_EXPECT_TRUE(_check_internal_content(), NULL);
#endif

#if TARGET_OS_IPHONE
	T_MAYFAIL;
	T_EXPECT_TRUE(_check_internal_release_type(), NULL);

	T_MAYFAIL;
	T_EXPECT_TRUE(_check_factory_release_type(), NULL);
#else
	T_MAYFAIL;
	T_EXPECT_FALSE(_check_internal_diags_profile(), NULL);

	T_MAYFAIL;
	T_EXPECT_FALSE(_check_factory_content(), NULL);
#endif

	T_MAYFAIL;
	T_EXPECT_TRUE(_check_can_has_debugger(), NULL);
}

T_DECL(os_variant_override_parse, "Checks the parsing of the override file")
{
	// Warm up the dispatch_once
	_check_disabled(VP_CONTENT);

	T_LOG("Override: NULL"); // Live system
	_parse_disabled_status(NULL);
	T_MAYFAIL; T_EXPECT_FALSE(_check_disabled(VP_CONTENT), NULL);
	T_MAYFAIL; T_EXPECT_FALSE(_check_disabled(VP_DIAGNOSTICS), NULL);
	T_MAYFAIL; T_EXPECT_FALSE(_check_disabled(VP_UI), NULL);
	T_MAYFAIL; T_EXPECT_FALSE(_check_disabled(VP_SECURITY), NULL);

	T_LOG("Override: \"content\"");
	_parse_disabled_status("content");
	T_EXPECT_TRUE(_check_disabled(VP_CONTENT), NULL);
	T_EXPECT_FALSE(_check_disabled(VP_DIAGNOSTICS), NULL);
	T_EXPECT_FALSE(_check_disabled(VP_UI), NULL);
	T_EXPECT_FALSE(_check_disabled(VP_SECURITY), NULL);

	T_LOG("Override: \"ui\"");
	_parse_disabled_status("ui");
	T_EXPECT_FALSE(_check_disabled(VP_CONTENT), NULL);
	T_EXPECT_FALSE(_check_disabled(VP_DIAGNOSTICS), NULL);
	T_EXPECT_TRUE(_check_disabled(VP_UI), NULL);
	T_EXPECT_FALSE(_check_disabled(VP_SECURITY), NULL);

	T_LOG("Override: \"security,diagnostics\"");
	_parse_disabled_status("security,diagnostics");
	T_EXPECT_FALSE(_check_disabled(VP_CONTENT), NULL);
	T_EXPECT_TRUE(_check_disabled(VP_DIAGNOSTICS), NULL);
	T_EXPECT_FALSE(_check_disabled(VP_UI), NULL);
	T_EXPECT_TRUE(_check_disabled(VP_SECURITY), NULL);

	T_LOG("Override: \"content,diagnostics,ui,security\"");
	_parse_disabled_status("content,diagnostics,ui,security");
	T_EXPECT_TRUE(_check_disabled(VP_CONTENT), NULL);
	T_EXPECT_TRUE(_check_disabled(VP_DIAGNOSTICS), NULL);
	T_EXPECT_TRUE(_check_disabled(VP_UI), NULL);
	T_EXPECT_TRUE(_check_disabled(VP_SECURITY), NULL);

	T_LOG("Override: \"diagnostics\\n"); // Now check newline-handling.
	_parse_disabled_status("diagnostics\n");
	T_EXPECT_FALSE(_check_disabled(VP_CONTENT), NULL);
	T_EXPECT_TRUE(_check_disabled(VP_DIAGNOSTICS), NULL);
	T_EXPECT_FALSE(_check_disabled(VP_UI), NULL);
	T_EXPECT_FALSE(_check_disabled(VP_SECURITY), NULL);

	T_LOG("Override: \"content,diagnostics\\nui,security\\n\"");
	_parse_disabled_status("content,diagnostics\nui,security\n");
	T_EXPECT_TRUE(_check_disabled(VP_CONTENT), NULL);
	T_EXPECT_TRUE(_check_disabled(VP_DIAGNOSTICS), NULL);
	T_EXPECT_TRUE(_check_disabled(VP_UI), NULL);
	T_EXPECT_TRUE(_check_disabled(VP_SECURITY), NULL);
}

T_DECL(os_status_cache, "Checks saving and restoring of state")
{
	uint64_t status = 0;
	size_t status_size = sizeof(status);
	int ret = sysctlbyname(CACHE_SYSCTL_NAME, &status, &status_size, NULL, 0);
	T_EXPECT_POSIX_ZERO(ret, "sysctlbyname(kern.osvariant_status)");
	T_EXPECT_GT(status, 0ULL, "Kernel's status has bits set");
	T_EXPECT_EQ(status & STATUS_INITIAL_BITS, STATUS_INITIAL_BITS, "Kernel's status has initial bits set");

	T_MAYFAIL;
	T_EXPECT_TRUE(_check_can_has_debugger(), NULL);

	status = _get_cached_check_status();
	T_LOG("Cached status: %llx", status);

	T_EXPECT_EQ(status & STATUS_INITIAL_BITS, STATUS_INITIAL_BITS, "Our status has initial bits set");

	_restore_cached_check_status(status);

	T_MAYFAIL;
	T_EXPECT_TRUE(os_variant_allows_internal_security_policies(NULL), NULL);

	status = STATUS_INITIAL_BITS |
			(S_NO << (SFP_CAN_HAS_DEBUGGER * STATUS_BIT_WIDTH)) |
			(S_NO << (SFP_DEVELOPMENT_KERNEL * STATUS_BIT_WIDTH));
	T_LOG("Restoring status without can_has_debugger and development_kernel: %llx", status);
	_restore_cached_check_status(status);

	T_EXPECT_FALSE(_check_can_has_debugger(), NULL);

	// Trigger dispatch_once internally with known state
	_check_disabled(VP_SECURITY);

	status = STATUS_INITIAL_BITS |
			(0x1ULL << (VP_SECURITY + 32));
	T_LOG("Restoring status with override: %llx", status);
	_restore_cached_check_status(status);

	T_EXPECT_TRUE(_check_disabled(VP_SECURITY), NULL);
}

// Ignore the last NULL entry in _variant_map.
#define VARIANTS_LEN (sizeof(_variant_map) / sizeof(_variant_map[0])) - 1

T_DECL(os_variant_asciibetical,
		"Check that the variant map is in asciibetical order")
{
	const char *prev_variant = _variant_map[0].variant;
	for (size_t i = 1; i < VARIANTS_LEN; i++) {
		const char *variant = _variant_map[i].variant;
		T_EXPECT_GT(strcmp(variant, prev_variant), 0,
				"Variant %s should be asciibetically after variant %s",
				variant, prev_variant);
		prev_variant = variant;
	}
}

T_DECL(os_variant_copy_description,
		"Check that the description matches what os_variant_check returns")
{
	T_SETUPBEGIN;
	struct {
		const char *variant;
		bool seen;
	} seen_variants[VARIANTS_LEN] = {};
	for (size_t i = 0; i < VARIANTS_LEN; i++) {
		seen_variants[i].variant = _variant_map[i].variant;
		T_QUIET; T_ASSERT_NOTNULL(seen_variants[i].variant,
				"copied variant %zu to test for", i);
	}
	T_SETUPEND;

	char *variant_desc = os_variant_copy_description("com.apple.Libc.tests");
	T_WITH_ERRNO; T_ASSERT_NOTNULL(variant_desc,
			"copied os_variant description");
	T_LOG("Got os_variant description: %s", variant_desc);

	char *next_variant = variant_desc;
	while (next_variant && next_variant[0] != '\0') {
		char *variant_end = strchr(next_variant, ' ');
		if (variant_end) {
			variant_end[0] = '\0';
			variant_end += 1;
		}
		for (size_t i = 0; i < VARIANTS_LEN; i++) {
			if (strcmp(next_variant, seen_variants[i].variant) == 0) {
				seen_variants[i].seen = true;
			}
		}
		T_EXPECT_TRUE(os_variant_check("com.apple.Libc.tests", next_variant),
				"Y: %s: os_variant_check should agree with description",
				next_variant);
		next_variant = variant_end;
	}

	for (size_t i = 0; i < VARIANTS_LEN; i++) {
		if (!seen_variants[i].seen) {
			T_EXPECT_FALSE(os_variant_check("com.apple.Libc.tests",
					seen_variants[i].variant),
					"N: %s: os_variant_check should return false for variant "
					"missing from description", seen_variants[i].variant);
		}
	}

	free(variant_desc);
}
