//
//  notify_exec.c
//  darwintests
//
//  Created by Brycen Wershing on 2/10/20.
//

#include <darwintest.h>
#include <notify.h>
#include <notify_private.h>
#include <dispatch/dispatch.h>
#include <stdlib.h>
#include <unistd.h>

extern char **environ;

T_DECL(notify_exec,
       "notify exec",
       T_META("owner", "Core Darwin Daemons & Tools"),
       T_META("as_root", "true"))
{
	pid_t pid = fork();

	if (pid == 0) {
		printf("Child started up\n");

		char *argv[3];
		argv[0] = "notify_test_helper";
		argv[1] = "Continue";
		argv[2] = NULL;

		execve("/AppleInternal/Tests/Libnotify/notify_test_helper" ,argv , environ);
		printf("execve failed with %d\n", errno);
		abort();
	} else {
		int status;
		T_LOG("Fork returned %d", pid);
		pid = waitpid(pid, &status, 0);
		if (pid == -1) {
			T_FAIL("wait4 failed with %d", errno);
			return;
		}

		if (!WIFEXITED(status)) {
			T_FAIL("Unexpected helper termination");
			return;
		}

		int exitStatus = WEXITSTATUS(status);

		if (exitStatus == 42) {
			T_PASS("Succeeded!");
		} else {
			T_FAIL("Failed with %d", exitStatus);
		}
	}
}
