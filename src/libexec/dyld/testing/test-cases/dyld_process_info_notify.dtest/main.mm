
// BUILD:  $CC target.c      -o $BUILD_DIR/target.exe -DRUN_DIR="$RUN_DIR"
// BUILD:  $CC foo.c        -o $BUILD_DIR/libfoo.dylib -dynamiclib
// BUILD:  $CXX main.mm     -o $BUILD_DIR/dyld_process_info_notify.exe -DRUN_DIR="$RUN_DIR"
// BUILD:  $TASK_FOR_PID_ENABLE $BUILD_DIR/dyld_process_info_notify.exe

// RUN:  $SUDO ./dyld_process_info_notify.exe

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <signal.h>
#include <spawn.h>
#include <errno.h>
#include <libgen.h>
#include <sys/proc.h>
#include <mach/mach.h>
#include <sys/param.h>
#include <mach/machine.h>
#include <mach-o/dyld_images.h>
#include <mach-o/dyld_process_info.h>
#include <dispatch/dispatch.h>
#include <Availability.h>

#include "test_support.h"

//FIXME: We need to add some concurrent access tests
//FIXME: Add cross architecture tests back now that arm64e macOS exists

extern char** environ;

// This is a one shot semaphore implementation that is QoS aware with integreated logging
struct OneShotSemaphore {
    OneShotSemaphore(const char* N) :_name(strdup(N)), _block(dispatch_block_create(DISPATCH_BLOCK_INHERIT_QOS_CLASS, ^{})) {}
    bool wait() {
        LOG("Waiting for semaphore %s", _name);
        dispatch_time_t tenSecondFromNow = dispatch_time(DISPATCH_WALLTIME_NOW, 10 * NSEC_PER_SEC);
        if (dispatch_block_wait(_block, tenSecondFromNow) != 0) {
            LOG("Timeout for semaphore %s", _name);
            return false;
        }
        return true;
    }
    void signal() {
        LOG("Signalling semaphore %s", _name);
        _block();
    }
private:
    const char*         _name;
    dispatch_block_t    _block;
};

void launchTest(bool launchSuspended, bool disconnectEarly)
{

    LOG("launchTest (%s)", launchSuspended ? "suspended" : "unsuspened");
    LOG("launchTest (%s)", disconnectEarly ? "disconnect early" : "normal disconnnect");
    dispatch_queue_t queue = dispatch_queue_create("com.apple.dyld.test.dyld_process_info", NULL);
    dispatch_queue_t signalQueue = dispatch_queue_create("com.apple.dyld.test.dyld_process_info.signals", NULL);

    // We use these blocks as semaphores. We do it this way so have ownership for QOS and so we get logging
    __block OneShotSemaphore childReady("childReady");
    __block OneShotSemaphore childExit("childExit");
    __block OneShotSemaphore childDone("childDone");
    __block OneShotSemaphore childExitNotification("childExitNotification");

    // We control our interactions with the sub ordinate process via signals, but if we send signals before its signal handlers
    // are installed it will terminate. We wait for it to SIGUSR1 us to indicate it is ready, so we need to setup a signal handler for
    // that.
    signal(SIGUSR1, SIG_IGN);
    dispatch_source_t usr1SignalSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGUSR1, 0, signalQueue);
    dispatch_source_set_event_handler(usr1SignalSource, ^{
        LOG("Got SIGUSR1");
        childReady.signal();
    });
    dispatch_resume(usr1SignalSource);

    signal(SIGUSR2, SIG_IGN);
    dispatch_source_t usr2SignalSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGUSR2, 0, signalQueue);
    dispatch_source_set_event_handler(usr2SignalSource, ^{
        LOG("Got SIGUSR2");
        childDone.signal();
    });
    dispatch_resume(usr2SignalSource);

    pid_t pid;
    task_t task;
    __block bool sawMainExecutable = false;
    __block bool sawlibSystem = false;
    __block bool gotMainNotice = false;
    __block bool gotMainNoticeBeforeAllInitialDylibs = false;
    __block bool gotFooNoticeBeforeMain = false;
    __block int libFooLoadCount = 0;
    __block int libFooUnloadCount = 0;
    __block dyld_process_info_notify handle;

    _process process;
    process.set_executable_path(RUN_DIR "/target.exe");
    const char* env[] = { "TEST_OUTPUT=None", NULL};
    process.set_env(env);
    process.set_launch_suspended(launchSuspended);
    process.set_exit_handler(^(pid_t pid) {
        // This is almost all logging code, the only functional element of it
        // is calling the childExit() semaphore
        int status = 0;
        int dispStatus = 0;
        (void)waitpid(pid, &status, 0);
        const char* exitType = "UNKNOWN";
        if (WIFEXITED(status)) {
            exitType = "exit()";
            dispStatus = WEXITSTATUS(status);
        }
        if (WIFSIGNALED(status)) {
            exitType = "signal";
            dispStatus = WTERMSIG(status);
        }
        LOG("DIED via %s (pid: %d, status: %d)", exitType, pid, dispStatus);
        childExit.signal();
    });

    // Launch process
    pid = process.launch();
    LOG("launchTest pid (%u)", pid);
    if ( task_read_for_pid(mach_task_self(), pid, &task) != KERN_SUCCESS ) {
        FAIL("task_read_for_pid()");
    }

    // Attach notifier
    kern_return_t kr;
    unsigned count = 0;
    do {
        handle = _dyld_process_info_notify( task, queue,
                                            ^(bool unload, uint64_t timestamp, uint64_t machHeader, const uuid_t uuid, const char* path) {
                                                LOG("Handler called");
                                                if ( strstr(path, "/target.exe") != NULL )
                                                    sawMainExecutable = true;
                                                if ( strstr(path, "/libSystem") != NULL )
                                                    sawlibSystem = true;
                                                if ( strstr(path, "/libfoo.dylib") != NULL ) {
                                                    if ( !gotMainNotice ) {
                                                        gotFooNoticeBeforeMain = true;
                                                    }
                                                    if ( unload ) {
                                                        ++libFooUnloadCount;
                                                    } else {
                                                        ++libFooLoadCount;
                                                        if (disconnectEarly) {
                                                            _dyld_process_info_notify_release(handle);
                                                        }
                                                    }
                                                }
                                            },
                                            ^{
                                                LOG("TERMINATED (pid: %d)", pid);
                                                childExitNotification.signal();
                                            },
                                            &kr);
        ++count;
        if ( handle == NULL )
            LOG("_dyld_process_info_notify() returned NULL, result=%d, count=%d", kr, count);
    } while ( (handle == NULL) && (count < 5) );
    LOG("launchTest handler registered");

    if ( handle == NULL ) {
        FAIL("Did not not get handle");
    }

    // if suspended attach main notifier and unsuspend
    if (launchSuspended) {
        // If the process starts suspended register for main(),
        // otherwise skip since this test is a race between
        // process setup and notification registration
        _dyld_process_info_notify_main(handle, ^{
                                                LOG("target entering main()");
                                                gotMainNotice = true;
                                                if ( !sawMainExecutable || !sawlibSystem )
                                                    gotMainNoticeBeforeAllInitialDylibs = true;
                                                });
        kill(pid, SIGCONT);
        LOG("Sent SIGCONT");
    }

    if (!childReady.wait()) {
        FAIL("Timed out waiting for child to signal it is ready");
    }
    kill(pid, SIGUSR1);
    LOG("Sent SIGUSR1");
    if (!childDone.wait()) {
        FAIL("Timed out waiting for child to finish dlopen()/dlclose() operations");
    }
    if (launchSuspended) {
        if ( !sawMainExecutable ) {
            FAIL("Did not get load notification of main executable");
        }
        if ( !gotMainNotice ) {
            FAIL("Did not get notification of main()");
        }
        if ( gotMainNoticeBeforeAllInitialDylibs ) {
            FAIL("Notification of main() arrived before all initial dylibs");
        }
        if ( gotFooNoticeBeforeMain ) {
            FAIL("Notification of main() arrived after libfoo load notice");
        }
        if ( !sawlibSystem ) {
            FAIL("Did not get load notification of libSystem");
        }
    }
    kill(pid, SIGTERM);
    LOG("Sent SIGTERM");
    if (!childExitNotification.wait()) {
        FAIL("Timed out waiting for child exit notification via _dyld_process_info_notify");
    }
    if ( disconnectEarly ) {
        if ( libFooLoadCount != 1 ) {
            FAIL("Got %d load notifications about libFoo instead of 1", libFooLoadCount);
        }
        if ( libFooUnloadCount != 0 ) {
            FAIL("Got %d unload notifications about libFoo instead of 1", libFooUnloadCount);
        }
    } else {
        if ( libFooLoadCount != 3 ) {
            FAIL("Got %d load notifications about libFoo instead of 3", libFooLoadCount);
        }
        if ( libFooUnloadCount != 3 ) {
            FAIL("Got %d unload notifications about libFoo instead of 3", libFooUnloadCount);
        }
    }
    if (!childExit.wait()) {
        FAIL("Timed out waiting for child cleanup");
    }

    // Tear down
    dispatch_source_cancel(usr1SignalSource);
    dispatch_source_cancel(usr2SignalSource);
    if (!disconnectEarly) {
        _dyld_process_info_notify_release(handle);
    }
}

static void testSelfAttach(void) {
    __block OneShotSemaphore teardownSempahore("self test teardownSempahore");
    __block bool dylibLoadNotified = false;
    kern_return_t kr = KERN_SUCCESS;
    dispatch_queue_t queue = dispatch_queue_create("com.apple.dyld.test.dyld_process_info.self-attach", NULL);
    dyld_process_info_notify handle = _dyld_process_info_notify(mach_task_self(), queue,
                                       ^(bool unload, uint64_t timestamp, uint64_t machHeader, const uuid_t uuid, const char* path) {
                                           if ( strstr(path, "/libfoo.dylib") != NULL ) {
                                               dylibLoadNotified = true;
                                           }
                                       },
                                                                ^{ teardownSempahore.signal(); },
                                       &kr);
    if ( handle == NULL ) {
        LOG("_dyld_process_info_notify() returned NULL, result=%d", kr);
    }
    void* h = dlopen(RUN_DIR "/libfoo.dylib", 0);
    dlclose(h);
    if (!dylibLoadNotified) {
        FAIL("testSelfAttach");
    }
    _dyld_process_info_notify_release(handle);
    teardownSempahore.wait();

    // Get the all image info
    task_dyld_info_data_t taskDyldInfo;
    mach_msg_type_number_t taskDyldInfoCount = TASK_DYLD_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&taskDyldInfo, &taskDyldInfoCount) != KERN_SUCCESS) {
        FAIL("Could not find all image info");
    }
    dyld_all_image_infos* infos = (dyld_all_image_infos*)taskDyldInfo.all_image_info_addr;

    // Find a slot for the right
    uint8_t notifySlot;
    for (uint8_t notifySlot = 0; notifySlot < DYLD_MAX_PROCESS_INFO_NOTIFY_COUNT; ++notifySlot) {
        if (infos->notifyPorts[notifySlot] != 0) {
            FAIL("Port array entry %u not cleaned up, expected 0, got %u", notifySlot, infos->notifyPorts[notifySlot]);
        }
    }
}

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    // test 1) attempt to monitor the monitoring process
    testSelfAttach();
    // test 2) launch test program suspended and wait for it to run to completion
    launchTest(true, false);
    // test 3) launch test program in unsuspended and wait for it to run to completion
    launchTest(false, false);
    // test 4) launch test program suspended and disconnect from it after the first dlopen() in target.exe
    launchTest(true, true);
    // test 5) launch test program unsuspended and disconnect from it after the first dlopen() in target.exe
    launchTest(false, true);

    PASS("Success");
}
