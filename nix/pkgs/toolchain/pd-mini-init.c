/*
 * pd-mini-init: PID 1 for the minimal armv6/armv7 image. Checks the primitives launchd
 * needs, one line before and after each, then execs the real launchd if it is present.
 */
#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <pthread.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static void
run(const char *path, char *const argv[])
{
	pid_t pid = fork();
	int status = 0;

	if (pid == 0) {
		execv(path, argv);
		fprintf(stderr, "pd-mini-init: exec %s failed\n", path);
		_exit(127);
	}
	if (pid > 0) {
		waitpid(pid, &status, 0);
	}
}

static void *
thread_main(void *arg)
{
	return arg;
}

static void
selftest(void)
{
	struct timeval t0, t1;
	int kq, rv;
	struct kevent ev;
	struct timespec ts = { 2, 0 };
	void *ret = NULL;
	pthread_t th;
	mach_port_t port = MACH_PORT_NULL;
	mach_msg_empty_rcv_t msg;
	pid_t pid;
	char *echo_argv[] = { "echo", "pd-selftest: posix_spawn child ran", NULL };

	printf("pd-selftest: usleep 100ms start\n");
	gettimeofday(&t0, NULL);
	usleep(100000);
	gettimeofday(&t1, NULL);
	printf("pd-selftest: usleep done after %ld us\n",
	    (long)((t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_usec - t0.tv_usec)));

	printf("pd-selftest: kqueue timer start\n");
	kq = kqueue();
	EV_SET(&ev, 1, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, 100, NULL);
	rv = kevent(kq, &ev, 1, &ev, 1, &ts);
	printf("pd-selftest: kqueue timer done rv %d filter %d\n", rv, rv > 0 ? ev.filter : 0);

	printf("pd-selftest: pthread start\n");
	rv = pthread_create(&th, NULL, thread_main, (void *)0x5a);
	if (rv == 0) {
		pthread_join(th, &ret);
	}
	printf("pd-selftest: pthread done rv %d ret %p\n", rv, ret);

	// Mach traps, each in a child so a bad return cannot take PID 1 with it
	for (int t = 0; t < 4; t++) {
		static const char *const what[] = {
			"mach_msg receive, timeout 0", "semaphore_timedwait 100ms",
			"mach_msg receive, timeout 100ms", "mach_msg receive, message queued",
		};
		int st = 0;

		printf("pd-selftest: %s start\n", what[t]);
		pid = fork();
		if (pid == 0) {
			kern_return_t kr;

			mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
			if (t == 1) {
				semaphore_t sem;
				mach_timespec_t wait = { 0, 100000000 };

				semaphore_create(mach_task_self(), &sem, SYNC_POLICY_FIFO, 0);
				kr = semaphore_timedwait(sem, wait);
			} else {
				if (t == 3) {
					mach_msg_header_t h = { 0 };

					mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
					h.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
					h.msgh_size = sizeof(h);
					h.msgh_remote_port = port;
					mach_msg(&h, MACH_SEND_MSG, sizeof(h), 0, MACH_PORT_NULL, 0, MACH_PORT_NULL);
				}
				kr = mach_msg(&msg.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg), port,
				    t == 0 ? 0 : 100, MACH_PORT_NULL);
			}
			printf("pd-selftest:   child: kr 0x%x\n", kr);
			_exit(0x42);
		}
		waitpid(pid, &st, 0);
		printf("pd-selftest: %s done, child %s %d\n", what[t],
		    WIFEXITED(st) ? "exit" : "signal", WIFEXITED(st) ? WEXITSTATUS(st) : WTERMSIG(st));
	}

	printf("pd-selftest: dispatch_after on a global queue start\n");
	dispatch_semaphore_t sem = dispatch_semaphore_create(0);
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC),
	    dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
		dispatch_semaphore_signal(sem);
	});
	rv = (int)dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC));
	printf("pd-selftest: dispatch_after done %s\n", rv == 0 ? "fired" : "TIMED OUT");

	printf("pd-selftest: posix_spawn start\n");
	rv = posix_spawn(&pid, "/bin/echo", NULL, NULL, echo_argv, environ);
	if (rv == 0) {
		waitpid(pid, NULL, 0);
	}
	printf("pd-selftest: posix_spawn done rv %d\n", rv);
}

int
main(int argc, char *argv[])
{
	int fd = open("/dev/console", O_RDWR);

	if (fd >= 0) {
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		if (fd > 2) {
			close(fd);
		}
	}
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("\npd-mini-init: PureDarwin userland is up (pid %d)\n", getpid());

	char *uname_argv[] = { "uname", "-a", NULL };
	run("/bin/uname", uname_argv);
	selftest();

	if (access("/sbin/launchd.real", X_OK) == 0) {
		printf("pd-mini-init: exec /sbin/launchd.real\n");
		argv[0] = "/sbin/launchd";
		execv("/sbin/launchd.real", argv);
		printf("pd-mini-init: exec launchd failed\n");
	}
	printf("pd-mini-init: idle\n");
	for (;;) {
		sleep(3600);
	}
}
