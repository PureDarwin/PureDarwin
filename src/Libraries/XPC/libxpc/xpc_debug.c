#include <xpc/xpc.h>
#include <sys/reason.h>
#include <CrashReporterClient.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char *xpc_api_misuse_reason = NULL;

__attribute__((visibility("hidden"), noreturn))
void xpc_api_misuse(const char *info, ...) {
	va_list ap;
	va_start(ap, info);
	vasprintf(&xpc_api_misuse_reason, info, ap);
	va_end(ap);

	int fd = open("/dev/console", O_WRONLY | O_NOCTTY);
	if (fd >= 0) {
		const char prefix[] = "PureDarwin libxpc misuse: ";
		write(fd, prefix, sizeof(prefix) - 1);
		if (xpc_api_misuse_reason) {
			write(fd, xpc_api_misuse_reason, strlen(xpc_api_misuse_reason));
		}
		write(fd, "\n", 1);
		close(fd);
	}

	CRSetCrashLogMessage(xpc_api_misuse_reason);
	abort_with_reason(OS_REASON_LIBXPC, 2, xpc_api_misuse_reason, OS_REASON_FLAG_GENERATE_CRASH_REPORT);
}

const char *xpc_debugger_api_misuse_info(void) {
	return xpc_api_misuse_reason;
}
