#include <dt_impl.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/sysctl.h>
#include <os/assumes.h>

#if TARGET_OS_IPHONE && (TARGET_OS_SIMULATOR || !TARGET_OS_IOS)
#define TARGET_SUPPORTS_NATIVE_SYSTEM 0
#include <crt_externs.h>
#include <pthread.h>
#include <spawn.h>
#include <signal.h>

#define	_PATH_BSHELL	"/bin/sh"
#define environ (*_NSGetEnviron())
#else
#define TARGET_SUPPORTS_NATIVE_SYSTEM 1
#endif /* TARGET_OS_IPHONE && (TARGET_OS_SIMULATOR || !TARGET_OS_IOS) */


static int
dt_pid_from_process_name(const char* name)
{
	/*
	 * Because of the statically cached array, this function is not thread safe.
	 * Most of this code is from [VMUProcInfo getProcessIds]
	 * The reason we don't call this code directly is because we only get the pids,
	 * and then we have to turn around and construct a VMUProcInfo for each pid to
	 * populate our proc table.
	 */

	static struct kinfo_proc *proc_buf = NULL;
	static int nprocs;

	if (proc_buf == NULL) {
		size_t buf_size;
		int mib[3] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL};
		int err;

		/*
		 * Try to find out how many processes are around so we can
		 * size the buffer appropriately.  sysctl's man page specifically suggests
		 * this approach, and says it returns a bit larger size than needed to handle
		 * any new processes created between then and now.
		 */
		err = sysctl(mib, 4, NULL, &buf_size, NULL, 0);

		if( (err < 0) && (err != ENOMEM)) {
			/*
			 * ENOMEM says we didn't have enough room for the entire buffer.
			 * We don't care about that, we're trying to get the size of a buffer
			 * suitable for getting all the data.
			 */
			xyerror(D_UNKNOWN, "Failure calling sysctl to get process list buffer size");
			return -1;
		}

		/*
		 * Now for the real access.  Create the buffer, and grab the
		 * data.
		 */
		proc_buf = (struct kinfo_proc *)calloc(1, buf_size);

		if(sysctl(mib, 4, proc_buf, &buf_size, NULL, 0) < 0) {
			//perror("Failure calling sysctl to get proc buf");
			free(proc_buf);
			return -1;
		}
		nprocs = (int)(buf_size / (sizeof(struct kinfo_proc)));
	}

	int i;
	for (i = 0; i < nprocs; i++) {
		if (strncmp(name, proc_buf[i].kp_proc.p_comm, MAXCOMLEN) == 0)
			return proc_buf[i].kp_proc.p_pid;
	}

	return -1;
}


dt_ident_t*
dt_macro_lookup(dt_idhash_t *macros, const char *name)
{
	dt_ident_t *idp;
	idp = dt_idhash_lookup(macros, name);
	if (idp != NULL) {
		return idp;
	}
	/*
	 * If the macro was not found, check whether we are trying
	 * an expression that starts with pid_, look up the pid of a process
	 * with that name, and if found, add a macro with that name
	 */
	else if (strncmp(name, "pid_", strlen("pid_")) == 0) {
		int pid = dt_pid_from_process_name(name + strlen("pid_"));
		if (pid != -1) {
			idp = dt_idhash_insert(macros, name + strlen("pid_"), DT_IDENT_SCALAR, 0, pid, _dtrace_prvattr, 0, &dt_idops_thaw, NULL, 0);
			return idp;
		}
		else {
			xyerror(D_PROC_NOT_FOUND, "Could not find process with name %s\n", name + strlen("pid_"));
		}
	}
	return NULL;
}


cpu_type_t
dtrace_str2arch(const char *str)
{
	int i;
	const struct {
		char *name;
		cpu_type_t type;
	} archs[] = {
		{"i386", CPU_TYPE_I386},
		{"x86_64", CPU_TYPE_X86_64},
		{"arm", CPU_TYPE_ARM},
		{"arm64", CPU_TYPE_ARM64},
		{"any", CPU_TYPE_ANY},
		{NULL, NULL}
	};

	for (i = 0; archs[i].name != NULL; i++) {
		if (strcmp(str, archs[i].name) == 0)
			return archs[i].type;
	}
	return (cpu_type_t)0;
}

int
dt_kernel_lp64(void)
{
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, 0 /* kernproc */ };
	struct kinfo_proc kp;
	size_t len = sizeof(kp);

	int ret = sysctl(mib, sizeof(mib) / sizeof(mib[0]), &kp, &len, NULL, 0);
	if (ret == -1) {
		/* default to 32-bit */
		return 0;
	}

	if (kp.kp_proc.p_flag & P_LP64) {
		return 1;
	}

	return 0;

}

int 
dt_system(const char *command)
{
#if TARGET_SUPPORTS_NATIVE_SYSTEM
	return (system(command));
#else
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	int rval;
	pid_t pid, savedpid;
	struct sigaction ign, intact, quitact;
	sigset_t newsigblock, oldsigblock, defaultsig;
	posix_spawnattr_t attr;
	short flags = POSIX_SPAWN_SETSIGMASK;
	const char *argv[] = {"sh", "-c", command, NULL};
	if (!command) {
		return (0);
	}

	rval = posix_spawnattr_init(&attr);
	if (rval != 0) {
		return (rval);
	}
	(void)sigemptyset(&defaultsig);

	/*
	 * Enforce mutual exclusion since we're messing with the signal handlers
	 * (and libc does it).
	 */
	rval = pthread_mutex_lock(&mutex);
	if (rval != 0) {
		(void)posix_spawnattr_destroy(&attr);
		return (rval);
	}
	/*
	 * Ignore SIGINT and SIGQUIT, block SIGCHLD. Remember to save
	 * existing signal dispositions.
	 */
	ign.sa_handler = SIG_IGN;
	(void)sigemptyset(&ign.sa_mask);
	ign.sa_flags = 0;
	(void)sigaction(SIGINT, &ign, &intact);
	if (intact.sa_handler != SIG_IGN) {
		sigaddset(&defaultsig, SIGINT);
		flags |= POSIX_SPAWN_SETSIGDEF;
	}
	(void)sigaction(SIGQUIT, &ign, &quitact);
	if (quitact.sa_handler != SIG_IGN) {
		sigaddset(&defaultsig, SIGQUIT);
		flags |= POSIX_SPAWN_SETSIGDEF;
	}
	(void)sigemptyset(&newsigblock);
	(void)sigaddset(&newsigblock, SIGCHLD);
	(void)sigprocmask(SIG_BLOCK, &newsigblock, &oldsigblock);
	(void)posix_spawnattr_setsigmask(&attr, &oldsigblock);
	if (flags & POSIX_SPAWN_SETSIGDEF) {
		(void)posix_spawnattr_setsigdefault(&attr, &defaultsig);
	}
	(void)posix_spawnattr_setflags(&attr, flags);

	rval = posix_spawn(&pid, _PATH_BSHELL, NULL, &attr,
				(char *const *)argv, environ);
	(void)posix_spawnattr_destroy(&attr);
	if (rval == 0) {
		savedpid = pid;
		do {
			pid = wait4(savedpid, &rval, 0, (struct rusage *)0);
		} while (pid == -1 && errno == EINTR);
		if (pid == -1){
			rval = -1;
		}
	} else if (rval == ENOMEM || rval == EAGAIN) { /* as if fork failed */
		rval = -1;
	} else {
		rval = W_EXITCODE(127, 0); /* couldn't exec shell */
	}

	(void)sigaction(SIGINT, &intact, NULL);
	(void)sigaction(SIGQUIT,  &quitact, NULL);
	(void)sigprocmask(SIG_SETMASK, &oldsigblock, NULL);
	rval = pthread_mutex_unlock(&mutex);
	os_assert(rval == 0);
	return (rval);
#endif /* TARGET_SUPPORTS_NATIVE_SYSTEM */
}

