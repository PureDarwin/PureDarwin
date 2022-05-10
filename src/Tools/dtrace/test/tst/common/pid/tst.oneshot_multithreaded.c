/*
 * ASSERTION:
 * 	To verify that oneshot probes can be fired in a process that have
 * 	multiple threads executing code on the page where the oneshot probe is
 * SECTION: oneshot provider
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NTHREADS 2

int ready_threads = 0;

pthread_mutex_t mutex;
pthread_cond_t cond;


/*
 * This requires f and g to be on the same page
 */
void f(void) {

}

void g(void) {

}

void barrier(void) {
	pthread_mutex_lock(&mutex);
	ready_threads++;
	if (ready_threads == NTHREADS) {
		pthread_cond_broadcast(&cond);
		pthread_mutex_unlock(&mutex);
	}
	else {
		pthread_cond_wait(&cond, &mutex);
		pthread_mutex_unlock(&mutex);
	}

}

void set_priority(void) {
	int policy, err;
	struct sched_param param;
	if ((err = pthread_getschedparam(pthread_self(), &policy, &param)) != 0) {
		fprintf(stderr, "could not set priority: cannot retrieve thread scheduling parameters: %d\n", err);
		exit(2);
	}
	// Make sure we run at a higher priority than dtrace
	param.sched_priority = 48;

	if ((err = pthread_setschedparam(pthread_self(), policy, &param))) {
		fprintf(stderr, "could not set thread priority: %d\n", err);
		exit(3);
	}

	// Prevent priority decay
	if ((err = pthread_set_fixedpriority_self())) {
		fprintf(stderr, "could not set thread scheduling to fixed: %d\n", err);
		exit(4);
	}
}

void* first_thread(void *arg) {
	set_priority();
	barrier();
	while (1) {
		g();
	}
}

void* second_thread(void *arg) {
#pragma unused(arg)
	barrier();
	usleep(10000);

	while (1) {
		f();
	}
}

int main(void) {
	int err;
	pthread_t *thread = NULL;
	if ((err = pthread_cond_init(&cond, NULL)) != 0) {
		fprintf(stderr, "error initialising condition variable: %d\n", err);
		return err;
	}
	if ((err = pthread_mutex_init(&mutex, NULL)) != 0) {
		fprintf(stderr, "error initialising mutex: %d\n", err);
		return err;
	}
	err = pthread_create(&thread, NULL, first_thread, NULL);
	if (err) {
		fprintf(stderr, "error starting first thread: %d\n", err);
	}
	err = pthread_create(&thread, NULL, second_thread, NULL);
	if (err) {
		fprintf(stderr, "error starting second thread: %d\n", err);
	}

	while (1) {

	}
}
