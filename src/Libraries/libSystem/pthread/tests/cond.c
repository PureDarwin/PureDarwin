#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <libkern/OSAtomic.h>

#include "darwintest_defaults.h"
#include <darwintest_utils.h>

struct context {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	pthread_cond_t ready_cond;
	long waiters;
	long count;
	bool ready;
	char _padding[7];
};


static void *wait_thread(void *ptr) {
	struct context *context = ptr;

	// tell producer thread that we are ready
	T_QUIET;
	T_ASSERT_POSIX_ZERO(pthread_mutex_lock(&context->mutex), "pthread_mutex_lock");
	context->ready = true;
	T_QUIET;
	T_ASSERT_POSIX_ZERO(pthread_cond_signal(&context->ready_cond), "pthread_cond_signal");

	bool loop = true;
	while (loop) {

		if (context->count > 0) {
			++context->waiters;
			T_QUIET;
			T_ASSERT_POSIX_ZERO(pthread_cond_wait(&context->cond, &context->mutex), "[%ld] pthread_rwlock_unlock", context->count);
			--context->waiters;
			--context->count;
		} else {
			loop = false;
		}

	}

	T_QUIET;
	T_ASSERT_POSIX_ZERO(pthread_mutex_unlock(&context->mutex), "[%ld] pthread_mutex_unlock", context->count);

	return NULL;
}

T_DECL(cond, "pthread_cond",
		T_META_ALL_VALID_ARCHS(YES), T_META_TIMEOUT(120), T_META_CHECK_LEAKS(NO))
{
	struct context context = {
		.cond = PTHREAD_COND_INITIALIZER,
		.mutex = PTHREAD_MUTEX_INITIALIZER,
		.ready_cond = PTHREAD_COND_INITIALIZER,
		.waiters = 0,
		.count = 50000 * dt_ncpu(),
		.ready = false,
	};
	int i;
	int res;
	int threads = 2;
	pthread_t p[threads];
	for (i = 0; i < threads; ++i) {
		T_QUIET;
		T_ASSERT_POSIX_ZERO(pthread_mutex_lock(&context.mutex), "pthread_mutex_lock");

		context.ready = false;

		T_QUIET;
		T_ASSERT_POSIX_ZERO(pthread_create(&p[i], NULL, wait_thread, &context), "pthread_create");

		do {
			// mutex will be dropped and allow consumer thread to acquire
			T_QUIET;
			T_ASSERT_POSIX_ZERO(pthread_cond_wait(&context.ready_cond, &context.mutex), "pthread_cond_wait");
		} while (context.ready == false);

		T_QUIET;
		T_ASSERT_POSIX_ZERO(pthread_mutex_unlock(&context.mutex), "pthread_mutex_lock");

		T_LOG("Thread %d ready.", i);
	}

	T_LOG("All threads ready.");

	long half = context.count / 2;

	bool loop = true;
	while (loop) {
		T_QUIET;
		T_ASSERT_POSIX_ZERO(pthread_mutex_lock(&context.mutex), "[%ld] pthread_mutex_lock", context.count);
		if (context.waiters) {
			char *str;
			if (context.count > half) {
				str = "pthread_cond_broadcast";
				res = pthread_cond_broadcast(&context.cond);
			} else {
				str = "pthread_cond_signal";
				res = pthread_cond_signal(&context.cond);
			}
			T_QUIET;
			T_ASSERT_POSIX_ZERO(res, "[%ld] %s", context.count, str);
		}
		if (context.count <= 0) {
			loop = false;
			T_PASS("Completed stres test successfully.");
		}

		T_QUIET;
		T_ASSERT_POSIX_ZERO(pthread_mutex_unlock(&context.mutex),
				"[%ld] pthread_mutex_unlock", context.count);
	}

	for (i = 0; i < threads; ++i) {
		T_ASSERT_POSIX_ZERO(pthread_join(p[i], NULL), NULL);
	}
}

#pragma mark invalid concurrent mutex use

// XXX ulock-based condvars don't detect concurrent waiting for now
#if 0

static pthread_cond_t concurrent_cond = PTHREAD_COND_INITIALIZER;

static void *
invalid_wait_thread(void *arg)
{
	pthread_mutex_t *mutex = arg;

	int rc = pthread_mutex_lock(mutex);
	T_ASSERT_POSIX_ZERO(rc, "lock mutex");

	while (true) {
		rc = pthread_cond_wait(&concurrent_cond, mutex);
		if (rc == EINVAL) {
			T_PASS("Detected EINVAL");
			T_END;
		} else {
			T_ASSERT_POSIX_ZERO(rc, "cond_wait");
		}
	}
}

T_DECL(cond_invalid_concurrent_mutex, "Detect concurrent waits with different mutexes as invalid")
{
	int rc;
	pthread_t threads[2];
	pthread_mutex_t mutexes[2] = {
		PTHREAD_MUTEX_INITIALIZER,
		PTHREAD_MUTEX_INITIALIZER,
	};

	for (int i = 0; i < 2; i++) {
		rc = pthread_create(&threads[i], NULL, invalid_wait_thread,
				&mutexes[i]);
		T_ASSERT_POSIX_ZERO(rc, "pthread_create");
	}

	// will not return
	pthread_join(threads[0], NULL);
}

#endif

#pragma mark mutex ping pong test

struct cond_mutex_ping_pong_ctx_s {
	pthread_mutex_t mutex;
	int arrived;
	int group;
};

static struct {
	pthread_mutex_t sync_mutex;
	pthread_cond_t sync_cond;
	int group;
	pthread_cond_t shared_cond;
} ping_pong = {
	.sync_mutex = PTHREAD_MUTEX_INITIALIZER,
	.sync_cond = PTHREAD_COND_INITIALIZER,
	.group = 0,
	.shared_cond = PTHREAD_COND_INITIALIZER,
};

#define PING_PONG_NGROUPS 2
#define PING_PONG_GROUP_NTHREADS 3
#define PING_PONG_ITERATIONS 5000

static void *
ping_pong_thread(void *arg)
{
	int rc;
	struct cond_mutex_ping_pong_ctx_s *ctx = arg;

	for (int i = 1; i < PING_PONG_ITERATIONS; i++) {
		if (i % 5000 == 0) {
			T_LOG("Iteration %d", i);
		}

		// wait for our turn to synchronize on the shared_cond barrier
		rc = pthread_mutex_lock(&ping_pong.sync_mutex);
		T_QUIET; T_ASSERT_POSIX_ZERO(rc, "lock sync_mutex");

		while (ping_pong.group != ctx->group) {
			rc = pthread_cond_wait(&ping_pong.sync_cond, &ping_pong.sync_mutex);
			T_QUIET; T_ASSERT_POSIX_ZERO(rc, "sync cond_wait");
		}

		rc = pthread_mutex_unlock(&ping_pong.sync_mutex);
		T_QUIET; T_ASSERT_POSIX_ZERO(rc, "unlock sync_mutex");

		rc = pthread_mutex_lock(&ctx->mutex);
		T_QUIET; T_ASSERT_POSIX_ZERO(rc, "lock mutex");

		ctx->arrived++;

		if (ctx->arrived == i * PING_PONG_GROUP_NTHREADS) {
			// end our turn with shared_cond
			rc = pthread_cond_broadcast(&ping_pong.shared_cond);
			T_QUIET; T_ASSERT_POSIX_ZERO(rc, "shared cond_broadcast");

			// let the next group begin
			rc = pthread_mutex_lock(&ping_pong.sync_mutex);
			T_QUIET; T_ASSERT_POSIX_ZERO(rc, "lock sync_mutex");

			ping_pong.group = (ping_pong.group + 1) % PING_PONG_NGROUPS;

			rc = pthread_mutex_unlock(&ping_pong.sync_mutex);
			T_QUIET; T_ASSERT_POSIX_ZERO(rc, "unlock sync_mutex");

			// for fun, do this broadcast outside the mutex
			rc = pthread_cond_broadcast(&ping_pong.sync_cond);
			T_QUIET; T_ASSERT_POSIX_ZERO(rc, "sync cond_broadcast");

		} else {
			while (ctx->arrived < i * PING_PONG_GROUP_NTHREADS) {
				rc = pthread_cond_wait(&ping_pong.shared_cond, &ctx->mutex);
				T_QUIET; T_ASSERT_POSIX_ZERO(rc, "shared cond_wait");

				// TODO: assert that you now hold the correct group mutex
			}
		}

		rc = pthread_mutex_unlock(&ctx->mutex);
		T_QUIET; T_ASSERT_POSIX_ZERO(rc, "unlock mutex");
	}

	return NULL;
}

T_DECL(cond_mutex_ping_pong, "Wait on the same condition variable with different mutexes",
		T_META_ENVVAR("PTHREAD_MUTEX_USE_ULOCK=1"))
{
	int rc;
	pthread_t threads[PING_PONG_NGROUPS][PING_PONG_GROUP_NTHREADS];
	struct cond_mutex_ping_pong_ctx_s ctxs[PING_PONG_NGROUPS];

	for (int i = 0; i < PING_PONG_NGROUPS; i++) {
		struct cond_mutex_ping_pong_ctx_s *ctx = &ctxs[i];
		*ctx = (struct cond_mutex_ping_pong_ctx_s){
			.mutex = PTHREAD_MUTEX_INITIALIZER,
			.group = i,
		};

		for (int j = 0; j < PING_PONG_GROUP_NTHREADS; j++) {
			rc = pthread_create(&threads[i][j], NULL, ping_pong_thread, ctx);
			T_ASSERT_POSIX_ZERO(rc, "pthread_create");
		}
	}

	for (int i = 0; i < PING_PONG_NGROUPS; i++) {
		for (int j = 0; j < PING_PONG_GROUP_NTHREADS; j++) {
			rc = pthread_join(threads[i][j], NULL);
			T_ASSERT_POSIX_ZERO(rc, "pthread_join");
		}
	}
}

#pragma mark signal_thread_np tests

static struct signal_thread_ctx_s {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool signaled;
} signal_thread_ctx = {
	.mutex = PTHREAD_MUTEX_INITIALIZER,
	.cond = PTHREAD_COND_INITIALIZER,
};

static void *
chosen_waiter(void *arg __unused)
{
	struct signal_thread_ctx_s *ctx = &signal_thread_ctx;

	int rc = pthread_mutex_lock(&ctx->mutex);
	T_ASSERT_POSIX_ZERO(rc, "chosen waiter lock");

	while (!ctx->signaled) {
		rc = pthread_cond_wait(&ctx->cond, &ctx->mutex);
		T_ASSERT_POSIX_ZERO(rc, "chosen waiter cond_wait");
	}

	T_PASS("chosen waiter woke");
	T_END;
}

static void *
other_waiter_thread(void *arg __unused)
{
	struct signal_thread_ctx_s *ctx = &signal_thread_ctx;

	int rc = pthread_mutex_lock(&ctx->mutex);
	T_ASSERT_POSIX_ZERO(rc, "other waiter lock");

	while (true) {
		rc = pthread_cond_wait(&ctx->cond, &ctx->mutex);
		T_ASSERT_POSIX_ZERO(rc, "other waiter cond_wait");
	}

	T_ASSERT_FAIL("Not reached");
	return NULL;
}

T_DECL(cond_signal_thread_np_waiting, "signal a specific thread that's waiting")
{
	int rc;
	struct signal_thread_ctx_s *ctx = &signal_thread_ctx;

	pthread_attr_t other_attr;
	rc = pthread_attr_init(&other_attr);
	T_QUIET; T_ASSERT_POSIX_ZERO(rc, "pthread_attr_init");

	rc = pthread_attr_set_qos_class_np(&other_attr,
			QOS_CLASS_USER_INTERACTIVE, 0);
	T_ASSERT_POSIX_ZERO(rc, "pthread_attr_set_qos_class_np");

	pthread_t other;
	rc = pthread_create(&other, &other_attr, other_waiter_thread, NULL);
	T_ASSERT_POSIX_ZERO(rc, "create other thread");

	pthread_t chosen;
	rc = pthread_create(&chosen, NULL, chosen_waiter, NULL);
	T_ASSERT_POSIX_ZERO(rc, "create chosen thread");

	T_LOG("Waiting for threads to wait");
	sleep(5);

	rc = pthread_mutex_lock(&ctx->mutex);
	T_ASSERT_POSIX_ZERO(rc, "lock mutex");

	ctx->signaled = true;

	rc = pthread_mutex_unlock(&ctx->mutex);
	T_ASSERT_POSIX_ZERO(rc, "unlock mutex");

	rc = pthread_cond_signal_thread_np(&ctx->cond, chosen);
	T_ASSERT_POSIX_ZERO(rc, "cond_signal_thread_np");

	pthread_join(chosen, NULL);
}

static void *
absent_chosen_waiter(void *arg __unused)
{
	T_LOG("chosen thread doing nothing forever");
	while (true) {
		sleep(100);
	}

	T_ASSERT_FAIL("Not reached");
	return NULL;
}

static void *
not_absent_waiter(void *arg __unused)
{
	struct signal_thread_ctx_s *ctx = &signal_thread_ctx;

	int rc = pthread_mutex_lock(&ctx->mutex);
	T_ASSERT_POSIX_ZERO(rc, "other waiter lock");

	while (!ctx->signaled) {
		rc = pthread_cond_wait(&ctx->cond, &ctx->mutex);
		T_ASSERT_POSIX_ZERO(rc, "other waiter cond_wait");
	}

	T_PASS("other waiter woke");
	T_END;
}

T_DECL(cond_signal_thread_np_not_waiting, "signal a specific thread that isn't waiting")
{
	int rc;
	struct signal_thread_ctx_s *ctx = &signal_thread_ctx;

	pthread_t other;
	rc = pthread_create(&other, NULL, not_absent_waiter, NULL);
	T_ASSERT_POSIX_ZERO(rc, "create other thread");

	pthread_t chosen;
	rc = pthread_create(&chosen, NULL, absent_chosen_waiter, NULL);
	T_ASSERT_POSIX_ZERO(rc, "create chosen thread");

	T_LOG("Waiting for threads to wait");
	sleep(5);

	rc = pthread_mutex_lock(&ctx->mutex);
	T_ASSERT_POSIX_ZERO(rc, "lock mutex");

	ctx->signaled = true;

	rc = pthread_mutex_unlock(&ctx->mutex);
	T_ASSERT_POSIX_ZERO(rc, "unlock mutex");

	rc = pthread_cond_signal_thread_np(&ctx->cond, chosen);
	T_ASSERT_POSIX_ZERO(rc, "cond_signal_thread_np");

	pthread_join(other, NULL);
}

#pragma mark cancel signal race test

static struct cancel_signal_race_context_s {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} cancel_signal_race_context = {
	.mutex = PTHREAD_MUTEX_INITIALIZER,
	.cond = PTHREAD_COND_INITIALIZER,
};

static void
cancelee_cleanup_handler(void *arg __unused)
{
	T_LOG("cancelee cleanup handler");

	struct cancel_signal_race_context_s *ctx = &cancel_signal_race_context;
	int rc = pthread_mutex_unlock(&ctx->mutex);
	T_ASSERT_POSIX_ZERO(rc, "cleanup mutex unlock");
}

static void *
cancelee_thread(void *arg __unused)
{
	struct cancel_signal_race_context_s *ctx = &cancel_signal_race_context;

	int rc = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	T_QUIET; T_ASSERT_POSIX_ZERO(rc, "disabled cancelation of cancelee thread");

	rc = pthread_mutex_lock(&ctx->mutex);
	T_ASSERT_POSIX_ZERO(rc, "cancelee lock");

	rc = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	if (rc) {
		// manual T_QUIET since we can't safely call into libdarwintest with
		// cancelation enabled
		T_ASSERT_POSIX_ZERO(rc, "cancelation re-enabled");
	}

	pthread_cleanup_push(cancelee_cleanup_handler, NULL);

	rc = pthread_cond_wait(&ctx->cond, &ctx->mutex);

	pthread_cleanup_pop(0);

	int rc2 = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	T_QUIET; T_ASSERT_POSIX_ZERO(rc2, "re-disabled cancelation of cancelee thread");

	// If we make it here we didn't manage to exercise the race, but that's
	// legal.
	T_ASSERT_POSIX_ZERO(rc, "cancelee woke from cond_wait");

	rc = pthread_mutex_unlock(&ctx->mutex);
	T_ASSERT_POSIX_ZERO(rc, "cancelee unlocked");

	return NULL;
}

static struct {
	int dummy;
} other_thread_timed_out;

static void *
other_racing_thread(void *arg __unused)
{
	struct cancel_signal_race_context_s *ctx = &cancel_signal_race_context;

	int rc = pthread_mutex_lock(&ctx->mutex);
	T_ASSERT_POSIX_ZERO(rc, "other lock");

	struct timespec ts = {
		.tv_sec = 10,
	};

	rc = pthread_cond_timedwait_relative_np(&ctx->cond, &ctx->mutex, &ts);

	int rc2 = pthread_mutex_unlock(&ctx->mutex);
	T_ASSERT_POSIX_ZERO(rc2, "other thread unlocked");

	if (rc == ETIMEDOUT) {
		T_LOG("other thread timed out");
		return &other_thread_timed_out;
	} else {
		// XXX if we change the algorithm in a way that can lead to spurious
		// wakeups then this logic might become invalid, but at this point it's
		// not possible
		T_ASSERT_POSIX_ZERO(rc, "other thread woke from wait");
		return NULL;
	}
}

T_DECL(cond_cancel_signal_race, "Validate waiter cancelation does not eat wakes",
		T_META_ENVVAR("PTHREAD_MUTEX_USE_ULOCK=1"))
{
	int rc;
	struct cancel_signal_race_context_s *ctx = &cancel_signal_race_context;

	pthread_attr_t cancelee_attr;
	rc = pthread_attr_init(&cancelee_attr);
	T_QUIET; T_ASSERT_POSIX_ZERO(rc, "pthread_attr_init");

	rc = pthread_attr_set_qos_class_np(&cancelee_attr,
			QOS_CLASS_USER_INTERACTIVE, 0);
	T_ASSERT_POSIX_ZERO(rc, "pthread_attr_set_qos_class_np");

	pthread_t cancelee;
	rc = pthread_create(&cancelee, &cancelee_attr, cancelee_thread, NULL);
	T_ASSERT_POSIX_SUCCESS(rc, "create cancelee");

	pthread_attr_t other_attr;
	rc = pthread_attr_init(&other_attr);
	T_QUIET; T_ASSERT_POSIX_ZERO(rc, "pthread_attr_init");

	rc = pthread_attr_set_qos_class_np(&other_attr,
			QOS_CLASS_USER_INITIATED, 0);
	T_ASSERT_POSIX_ZERO(rc, "pthread_attr_set_qos_class_np");

	pthread_t other;
	rc = pthread_create(&other, &other_attr, other_racing_thread, NULL);
	T_ASSERT_POSIX_SUCCESS(rc, "create other thread");

	// Give them time to wait
	// TODO: find some reliable way of waiting until they're really blocked?
	sleep(2);

	rc = pthread_cond_signal(&ctx->cond);

	// Now quickly cancel, hopefully before they make it to userspace
	(void)pthread_cancel(cancelee);

	T_ASSERT_POSIX_ZERO(rc, "signal cancelee");

	void *cancelee_retval, *other_retval;

	rc = pthread_join(cancelee, &cancelee_retval);
	T_ASSERT_POSIX_ZERO(rc, "join cancelee");

	rc = pthread_join(other, &other_retval);
	T_ASSERT_POSIX_ZERO(rc, "join other");

	if (cancelee_retval == PTHREAD_CANCELED) {
		T_LOG("cancelee was canceled");
		T_ASSERT_EQ(other_retval, NULL, "other thread must have woken");
	} else {
		T_LOG("cancelee was not canceled quickly enough");
		T_ASSERT_EQ(cancelee_retval, NULL, "cancelee returned success");
		T_ASSERT_EQ(other_retval, &other_thread_timed_out, "other thread timed out");
	}
}
