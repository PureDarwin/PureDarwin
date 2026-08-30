#include <dispatch/dispatch.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <sys/code_signing.h>
#include <sys/kern_sysctl.h>
#include <sys/sysctl.h>
#include <sys/kern_memorystatus.h>

#include <darwintest.h>
#include <darwintest_utils.h>

#include "test_utils.h"

bool
is_development_kernel(void)
{
	static dispatch_once_t is_development_once;
	static bool is_development;

	dispatch_once(&is_development_once, ^{
		int dev;
		size_t dev_size = sizeof(dev);

		T_QUIET;
		T_ASSERT_POSIX_SUCCESS(sysctlbyname("kern.development", &dev,
		&dev_size, NULL, 0), NULL);
		is_development = (dev != 0);
	});

	return is_development;
}

bool
is_sptm_enabled(void)
{
	static dispatch_once_t is_sptm_enabled_once;
	static bool is_sptm_enabled;

	dispatch_once(&is_sptm_enabled_once, ^{
		int page_protection_type;
		size_t size = sizeof(page_protection_type);
		int err = sysctlbyname("kern.page_protection_type", &page_protection_type, &size, NULL, 0);
		T_ASSERT_POSIX_SUCCESS(err, "sysctl(\"kern.page_protection_type\");");
		is_sptm_enabled = (page_protection_type == 2);
	});

	return is_sptm_enabled;
}

bool
is_map_jit_allowed(void)
{
	static dispatch_once_t is_map_jit_allowed_once;
	static bool is_map_jit_allowed;

	dispatch_once(&is_map_jit_allowed_once, ^{
		code_signing_config_t cs_config = 0;
		size_t cs_config_size = sizeof(cs_config);

		/* Query the code signing configuration information */
		int err = sysctlbyname("security.codesigning.config", &cs_config, &cs_config_size, NULL, 0);
		T_ASSERT_POSIX_SUCCESS(err, "sysctl(\"security.codesigning.config\");");

		is_map_jit_allowed = !!(cs_config & CS_CONFIG_MAP_JIT);
	});

	return is_map_jit_allowed;
}

bool
process_is_translated()
{
	static dispatch_once_t is_translated_once;
	static bool is_translated;

	dispatch_once(&is_translated_once, ^{
		int out_value = 0;
		size_t inout_size = sizeof(out_value);
		if (sysctlbyname("sysctl.proc_translated", &out_value, &inout_size, NULL, 0) != 0) {
		        /*
		         * ENOENT means the sysctl is not present and therefore
		         * this process is not translated. Any other error is bad.
		         */
		        T_QUIET; T_ASSERT_POSIX_ERROR(errno, ENOENT, "sysctlbyname(sysctl.proc_translated)");
		        is_translated = false;
		} else {
		        T_QUIET; T_ASSERT_GE(inout_size, sizeof(out_value), "sysctlbyname(sysctl.proc_translated)");
		        is_translated = (bool)out_value;
		}
	});
	return is_translated;
}


pid_t
launch_background_helper(
	const char* variant,
	bool start_suspended,
	bool memorystatus_managed)
{
	pid_t pid;
	char **launch_tool_args;
	char testpath[PATH_MAX];
	char *variant_cpy = strdup(variant);
	uint32_t testpath_buf_size;
	int ret;

	testpath_buf_size = sizeof(testpath);
	ret = _NSGetExecutablePath(testpath, &testpath_buf_size);
	launch_tool_args = (char *[]){
		testpath,
		"-n",
		variant_cpy,
		NULL
	};
	ret = dt_launch_tool(&pid, launch_tool_args, start_suspended, NULL, NULL);
	if (ret != 0) {
		T_LOG("dt_launch tool returned %d with error code %d", ret, errno);
	}
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "dt_launch_tool");
	if (memorystatus_managed) {
		set_process_memorystatus_managed(pid);
	}
	free(variant_cpy);
	return pid;
}

void
set_process_memorystatus_managed(pid_t pid)
{
	kern_return_t ret = memorystatus_control(MEMORYSTATUS_CMD_SET_PROCESS_IS_MANAGED, pid, 1, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "memorystatus_control");
}
