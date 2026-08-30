#include <stdlib.h>
#include <stdio.h>
#include <mach/error.h>
#include <mach/task_info.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/vm_statistics.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/kern_memorystatus.h>
#include <sys/sysctl.h>
#include <stdatomic.h>

#include <darwintest.h>
#include <TargetConditionals.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vm"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("VM"));

#define KB 1024
#define VM_SIZE_PER_THREAD (64 * KB)

static _Atomic int thread_malloc_count = 0;
static _Atomic int thread_compressed_count = 0;
static _Atomic int thread_thawed_count = 0;
static _Atomic int phase = 0;

struct thread_args {
	int    id;
};

static void *
worker_thread_function(void *args)
{
	struct thread_args *targs = args;
	int thread_id = targs->id;
	char *array;

	/* Allocate memory */
	mach_vm_address_t addr;
	kern_return_t kr;
	kr = mach_vm_allocate(mach_task_self(), &addr, VM_SIZE_PER_THREAD,
	    VM_FLAGS_ANYWHERE | VM_PROT_DEFAULT | VM_MAKE_TAG(VM_MEMORY_APPLICATION_SPECIFIC_1));
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_vm_allocate()");
	array = (char *)addr;
	T_QUIET; T_EXPECT_NOTNULL(array, "thread %d allocated heap memory to be dirtied", thread_id);

	/* Waiting for phase 1 (touch pages) to start */
	while (atomic_load(&phase) != 1) {
		;
	}

	/* Phase 1: touch pages */
	T_LOG("thread %d phase 1: dirtying %d heap pages (%d bytes)", thread_id, VM_SIZE_PER_THREAD / (int)PAGE_SIZE, VM_SIZE_PER_THREAD);
	memset(&array[0], 1, VM_SIZE_PER_THREAD);
	atomic_fetch_add(&thread_malloc_count, 1);

	/* Wait for process to be frozen */
	while (atomic_load(&phase) != 2) {
		;
	}

	/* Phase 2: compress pages */
	kr = mach_vm_behavior_set(mach_task_self(), addr, VM_SIZE_PER_THREAD, VM_BEHAVIOR_PAGEOUT);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_vm_behavior_set()");
	atomic_fetch_add(&thread_compressed_count, 1);

	while (atomic_load(&phase) != 3) {
		;
	}

	/* Phase 3, process thawed, trigger decompressions by re-faulting pages */
	T_LOG("thread %d phase 3: faulting pages back in to trigger decompressions", thread_id);
	memset(&array[0], 1, VM_SIZE_PER_THREAD);

	/* Main thread will retrieve vm statistics once all threads are thawed */
	atomic_fetch_add(&thread_thawed_count, 1);

	kr = mach_vm_deallocate(mach_task_self(), addr, VM_SIZE_PER_THREAD);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_vm_deallocate()");

	return NULL;
}

static pthread_t*
create_threads(int nthreads, pthread_t *threads, struct thread_args *targs)
{
	int i;
	int err;
	pthread_attr_t attr;

	err = pthread_attr_init(&attr);
	T_ASSERT_POSIX_ZERO(err, "pthread_attr_init");
	for (i = 0; i < nthreads; i++) {
		targs[i].id = i;
		err = pthread_create(&threads[i], &attr, worker_thread_function, (void*)&targs[i]);
		T_QUIET; T_ASSERT_POSIX_ZERO(err, "pthread_create");
	}

	return threads;
}

static void
join_threads(int nthreads, pthread_t *threads)
{
	int i;
	int err;

	for (i = 0; i < nthreads; i++) {
		err = pthread_join(threads[i], NULL);
		T_QUIET; T_ASSERT_POSIX_ZERO(err, "pthread_join");
	}
}

T_DECL(task_vm_info_decompressions,
    "Test multithreaded per-task decompressions counter", T_META_TAG_VM_NOT_ELIGIBLE)
{
	int     err;
	mach_error_t kr;
	int     ncpu;
	size_t  ncpu_size = sizeof(ncpu);
	int     npages;
	int     compressor_mode;
	size_t  compressor_mode_size = sizeof(compressor_mode);
	task_vm_info_data_t vm_info;
	mach_msg_type_number_t count;
	pthread_t *threads;
	struct thread_args *targs;

	T_SETUPBEGIN;

	/* Make sure freezer is enabled on target machine */
	err = sysctlbyname("vm.compressor.mode", &compressor_mode, &compressor_mode_size, NULL, 0);
	if (compressor_mode < 8) {
		T_SKIP("This test requires freezer which is not available on the testing platform (vm.compressor.mode is set to %d)", compressor_mode);
	}
#if TARGET_OS_BRIDGE
	T_SKIP("This test requires freezer which is not available on bridgeOS (vm.compressor.mode is set to %d)", compressor_mode);
#endif

	/* Set number of threads to ncpu available on testing device */
	err = sysctlbyname("hw.ncpu", &ncpu, &ncpu_size, NULL, 0);
	T_EXPECT_EQ_INT(0, err, "Detected %d cpus\n", ncpu);

	/* Set total number of pages to be frozen */
	npages = ncpu * VM_SIZE_PER_THREAD / (int)PAGE_SIZE;
	T_LOG("Test will be freezing at least %d heap pages\n", npages);

	/* Change state to freezable */
	err = memorystatus_control(MEMORYSTATUS_CMD_SET_PROCESS_IS_FREEZABLE, getpid(), (uint32_t)1, NULL, 0);
	T_EXPECT_EQ(KERN_SUCCESS, err, "set pid %d to be freezable", getpid());

	/* Call into kernel to retrieve vm_info and make sure we do not have any decompressions before the test */
	count = TASK_VM_INFO_COUNT;
	err = task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&vm_info, &count);
	T_EXPECT_EQ(count, TASK_VM_INFO_COUNT, "count == TASK_VM_INFO_COUNT: %d", count);
	T_EXPECT_EQ_INT(0, err, "task_info(TASK_VM_INFO) returned 0");
	T_EXPECT_EQ_INT(0, vm_info.decompressions, "Expected 0 decompressions before test starts");

	/* Thread data */
	threads = malloc(sizeof(pthread_t) * (size_t)ncpu);
	targs = malloc(sizeof(struct thread_args) * (size_t)ncpu);

	T_SETUPEND;

	/* Phase 1: create threads to write to malloc memory */
	create_threads(ncpu, threads, targs);
	atomic_fetch_add(&phase, 1);

	/* Wait for all threads to dirty their malloc pages */
	while (atomic_load(&thread_malloc_count) != ncpu) {
		sleep(1);
	}
	T_EXPECT_EQ(ncpu, atomic_load(&thread_malloc_count), "%d threads finished writing to malloc pages\n", ncpu);

	count = TASK_VM_INFO_COUNT;
	err = task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&vm_info, &count);
	T_QUIET; T_EXPECT_EQ(count, TASK_VM_INFO_COUNT, "count == TASK_VM_INFO_COUNT: %d", count);
	T_QUIET; T_EXPECT_EQ_INT(0, err, "task_info(TASK_VM_INFO) returned 0");
	T_EXPECT_EQ(0, vm_info.decompressions, "Expected 0 decompressions before compressions");

	/* Launch freezer to compress the dirty pages */
	atomic_fetch_add(&phase, 1);
	/* Wait for all threads to compress their pages */
	while (atomic_load(&thread_compressed_count) != ncpu) {
		sleep(1);
	}
	T_EXPECT_EQ(ncpu, atomic_load(&thread_compressed_count), "%d threads finished writing to malloc pages\n", ncpu);

	/* Phase 2: triger decompression in threads */
	atomic_fetch_add(&phase, 1);

	/* Wait for all threads to decompress their malloc pages */
	while (atomic_load(&thread_thawed_count) != ncpu) {
		sleep(1);
	}

	/* Phase 3: Call into kernel to retrieve vm_info and to get the updated decompressions counter */
	count = TASK_VM_INFO_COUNT;
	kr = task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&vm_info, &count);
	T_QUIET; T_EXPECT_EQ(count, TASK_VM_INFO_COUNT, "count == TASK_VM_INFO_COUNT: %d", count);
	T_QUIET; T_EXPECT_MACH_SUCCESS(kr, "task_info(TASK_VM_INFO)");

	/* Make sure this task has decompressed at least all of the dirtied memory */
	T_EXPECT_GE_INT(vm_info.decompressions, npages, "decompressed %d pages (>= heap pages: %d)", vm_info.decompressions, npages);
	T_PASS("Correctly retrieve per-task decompressions stats");

	/* Cleanup */
	join_threads(ncpu, threads);
	free(threads);
	free(targs);
}
