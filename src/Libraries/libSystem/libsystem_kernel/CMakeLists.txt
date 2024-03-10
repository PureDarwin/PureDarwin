# libsystem_kernel has a few targets:
# - /usr/local/lib/dyld/libsystem_kernel.a
#   - 3 public, 4 private headers
#     - wrappers/gethostuuid.h
#     - wrappers/spawn/spawn.h
#     - wrappers/libproc/libproc.h
#
#     - wrappers/gethostuuid_private.h
#     - wrappers/_libkernel_init.h
#     - wrappers/spawn/spawn_private.h
#     - wrappers/libproc/libproc_internal.h
#   - 2 headers in private os/
#     - os/thread_self_restrict.h
#     - os/tsd.h
#   - compile 140 sources
#   - public mig headers (mach/, servers/)
#   - private mig headers (mach/)
#   - link with syscalls.a
#
# - /usr/lib/system/libsystem_kernel.dylib
#   - link with libsystem_kernel.a
#   - compile 10 sources
#
# - /usr/local/lib/system/libsystem_kernel.a (copy of /usr/local/lib/dyld/libsystem_kernel.a)
# - /usr/local/lib/loaderd/libsystem_kernel.a (copy of /usr/local/lib/dyld/libsystem_kernel.a)
# - /usr/local/lib/dyld_stub/libsystem_kernel.a (copy of /usr/local/lib/dyld/libsystem_kernel.a)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/syscalls.a
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/sys
    COMMAND ${CMAKE_COMMAND} -E env ARCHS=x86_64
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/create-syscalls.pl
        ${PUREDARWIN_SOURCE_DIR}/src/Kernel/xnu/bsd/kern/syscalls.master
        ${CMAKE_CURRENT_SOURCE_DIR}/custom ${CMAKE_CURRENT_SOURCE_DIR}/Platforms
        MacOSX ${CMAKE_CURRENT_BINARY_DIR}/sys
    COMMAND ${CMAKE_COMMAND} -E env ARCHS=x86_64
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/compile-syscalls.pl
        ${CMAKE_CURRENT_BINARY_DIR}/sys/stubs.list ${CMAKE_CURRENT_BINARY_DIR}/syscalls.a
        -I${PUREDARWIN_BINARY_DIR}/src/Kernel/xnu/xnu_header_install/usr/include
        -I${PUREDARWIN_BINARY_DIR}/src/Kernel/xnu/xnu_header_install/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders
        -I${PUREDARWIN_SOURCE_DIR}/src/Kernel/xnu/EXTERNAL_HEADERS
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/create-syscalls.pl
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/compile-syscalls.pl
        ${PUREDARWIN_SOURCE_DIR}/src/Kernel/xnu/bsd/kern/syscalls.master
    COMMENT "Compile syscall stubs" VERBATIM
)

add_library(libsyscall_traps STATIC IMPORTED)
set_property(TARGET libsyscall_traps PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/syscalls.a)

# END SYSCALL TRAPS

add_library(libsystem_kernel_private_headers INTERFACE)
target_include_directories(libsystem_kernel_private_headers INTERFACE private_include)

# END INTERFACE DEFINITION

add_darwin_static_library(libsystem_kernel_static)

# This product should be called libsystem_kernel.a and should be placed in /usr/local/lib/{dyld,system,loaderd,dyld_stub}
set_target_properties(libsystem_kernel_static PROPERTIES OUTPUT_NAME "system_kernel")

target_sources(libsystem_kernel_static PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/syscalls.a

    mach/clock_sleep.c
    mach/error_codes.c
    mach/exc_catcher_state_identity.c
    mach/exc_catcher_state.c
    mach/exc_catcher.c
    mach/fprintf_stderr.c
    mach/host.c
    mach/mach_error_string.c
    mach/mach_error.c
    mach/mach_eventlink.c
    mach/mach_init.c
    mach/mach_legacy.c
    mach/mach_msg.c
    mach/mach_port.c
    mach/mach_right.c
    mach/mach_traps.s
    mach/mach_vm.c
    mach/mig_allocate.c
    mach/mig_deallocate.c
    mach/mig_reply_port.c
    mach/mig_reply_setup.c
    mach/mig_strncpy.c
    mach/ms_thread_switch.c
    mach/panic.c
    mach/port_descriptions.c
    mach/port_obj.c
    mach/semaphore.c
    mach/slot_name.c
    mach/string.c
    mach/task.c
    mach/thread_act.c

    wrappers/__commpage_gettimeofday.c
    wrappers/__get_cpu_capabilities.s
    wrappers/carbon_delete.c
    wrappers/clonefile.c
    wrappers/coalition.c
    wrappers/csr.c
    wrappers/fs_snapshot.c
    wrappers/gethostuuid.c
    wrappers/getiopolicy_np.c
    wrappers/guarded_open_dprotected_np.c
    wrappers/guarded_open_np.c
    wrappers/init_cpu_capabilities.c
    wrappers/ioctl.c
    wrappers/kdebug_trace.c
    wrappers/kill.c
    wrappers/mach_absolute_time.s
    wrappers/mach_approximate_time.c
    wrappers/mach_approximate_time.s
    wrappers/mach_boottime.c
    wrappers/mach_bridge_remote_time.c
    wrappers/mach_continuous_time.c
    wrappers/mach_get_times.c
    wrappers/mach_timebase_info.c
    wrappers/open-base.c
    wrappers/open_dprotected_np.c
    wrappers/persona.c
    wrappers/pid_shutdown_networking.c
    wrappers/posix_sem_obsolete.c
    wrappers/proc.c
    wrappers/quota_obsolete.c
    wrappers/reboot.c
    wrappers/remove-counter.c
    wrappers/rename.c
    wrappers/renameat.c
    wrappers/renamex.c
    wrappers/rmdir.c
    wrappers/setpriority.c
    wrappers/sfi.c
    wrappers/stackshot.c
    wrappers/system-version-compat-support.c
    wrappers/terminate_with_reason.c
    wrappers/thread_register_state.c
    wrappers/unlink.c
    wrappers/unlinkat.c
    wrappers/utimensat.c
    wrappers/varargs_wrappers.s
    wrappers/work_interval.c

    wrappers/cancelable/fcntl.c
    wrappers/cancelable/fcntl-cancel.c
    wrappers/cancelable/open.c
    wrappers/cancelable/pselect-darwinext.c
    wrappers/cancelable/pselect-darwinext-cancel.c
    wrappers/cancelable/select-cancel.c
    wrappers/cancelable/select.c
    wrappers/cancelable/sigsuspend.c
    wrappers/cancelable/sigsuspend-cancel.c

    wrappers/legacy/accept.c
    wrappers/legacy/bind.c
    wrappers/legacy/connect.c
    wrappers/legacy/getattrlist.c
    wrappers/legacy/getaudit.c
    wrappers/legacy/getpeername.c
    wrappers/legacy/getsockname.c
    wrappers/legacy/kill.c
    wrappers/legacy/lchown.c
    wrappers/legacy/listen.c
    wrappers/legacy/mprotect.c
    wrappers/legacy/msync.c
    wrappers/legacy/munmap.c
    wrappers/legacy/recvfrom.c
    wrappers/legacy/recvmsg.c
    wrappers/legacy/select.c
    wrappers/legacy/select-pre1050.c
    wrappers/legacy/sendmsg.c
    wrappers/legacy/sendto.c
    wrappers/legacy/setattrlist.c
    wrappers/legacy/sigsuspend.c
    wrappers/legacy/socketpair.c

    wrappers/libproc/libproc.c
    wrappers/libproc/proc_listpidspath.c
    wrappers/spawn/posix_spawn.c

    wrappers/unix03/chmod.c
    wrappers/unix03/fchmod.c
    wrappers/unix03/getrlimit.c
    wrappers/unix03/mmap.c
    wrappers/unix03/munmap.c
    wrappers/unix03/setrlimit.c

    custom/errno.c

    os/alloc_once.c
    os/log_data.c
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/public_include/servers)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach)

# libc exposes some public headers we use even though we don't need to link against libc
target_link_libraries(libsystem_kernel_static PUBLIC libc_public_headers)

# We need these headers
target_link_libraries(libsystem_kernel_static PRIVATE clang_bootstrap_headers)
target_link_libraries(libsystem_kernel_static PUBLIC architecture_headers)

target_include_directories(libsystem_kernel_static PUBLIC include ${CMAKE_CURRENT_BINARY_DIR}/public_include)
target_include_directories(libsystem_kernel_static PRIVATE mach mach/mach mach/servers custom wrappers)
target_include_directories(libsystem_kernel_static PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/internal_include)

target_compile_options(libsystem_kernel_static PRIVATE -fdollars-in-identifiers -no-cpp-precomp -fno-common -fno-stack-protector -fno-stack-check -momit-leaf-frame-pointer -Wno-nullability-completeness -Wno-implicit-function-declaration)

# We define LIBSYSCALL_INTERFACE to pass to the MIG compiler below. This will indicate we don't want 'internal' versions of the given MIG files.
target_compile_definitions(libsystem_kernel_static PRIVATE _FORTIFY_SOURCE=0 __DARWIN_VERS_1050=1 NO_SYSCALL_LEGACY SYSCALL_PRE1050 LIBSYSCALL_INTERFACE)

target_link_libraries(libsystem_kernel_static PRIVATE xnu_private_headers libsystem_kernel_private_headers libsyscall_traps)
target_link_libraries(libsystem_kernel_static PUBLIC xnu_headers pthread_common_headers AvailabilityHeaders)

# Public header generation (no source files here due to macro definition issues)
mig(mach/clock.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/clock.h NOVOUCHERS)
mig(mach/clock_priv.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/clock_priv.h NOVOUCHERS)
mig(mach/clock_reply.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/clock_reply.h NOVOUCHERS)
mig(mach/exc.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/exc.h NOVOUCHERS)
mig(mach/host_priv.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/host_priv.h NOVOUCHERS)
mig(mach/host_security.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/host_security.h NOVOUCHERS)
mig(mach/lock_set.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/lock_set.h NOVOUCHERS)
mig(mach/mach_host.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_host.h NOVOUCHERS)
mig(mach/mach_port.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_port.h NOVOUCHERS)
mig(mach/mach_voucher.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_voucher.h NOVOUCHERS)
mig(mach/mach_vm.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_vm.h NOVOUCHERS)
mig(mach/memory_entry.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/memory_entry.h NOVOUCHERS)
mig(mach/processor.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/processor.h NOVOUCHERS)
mig(mach/processor_set.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/processor_set.h NOVOUCHERS)
mig(mach/task.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/task.h NOVOUCHERS)
mig(mach/thread_act.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/thread_act.h NOVOUCHERS)
mig(mach/vm_map.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/vm_map.h NOVOUCHERS)

mig(mach/servers/netname.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/servers/netname.h NOVOUCHERS)

# Pass NODEFINES to build 'internal' copies of these MIG files. See scripts/mach_install_mig.sh for info.
mig(mach/clock.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/clock.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/clock.h NOVOUCHERS)
mig(mach/clock_priv.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/clock_priv.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/clock_priv.h NOVOUCHERS)
mig(mach/clock_reply.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/clock_reply.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/clock_reply.h NOVOUCHERS)
mig(mach/host_priv.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/host_priv.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/host_priv.h NOVOUCHERS)
mig(mach/host_security.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/host_security.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/phost_security.h NOVOUCHERS)
mig(mach/lock_set.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/lock_set.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/lock_set.h NOVOUCHERS)
mig(mach/mach_eventlink.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_eventlink.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mach_eventlink.h NOVOUCHERS)
mig(mach/mach_host.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_host.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mach_host.h NOVOUCHERS)
mig(mach/mach_voucher.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_voucher.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mach_voucher.h NOVOUCHERS)
mig(mach/mach_vm.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_vm.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mach_vm.h NOVOUCHERS)
mig(mach/memory_entry.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/memory_entry.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/memory_entry.h NOVOUCHERS)
mig(mach/notify.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/notify.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/notify.h NOVOUCHERS)
mig(mach/processor.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/processor.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/processor.h NOVOUCHERS)
mig(mach/processor_set.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/processor_set.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/processor_set.h NOVOUCHERS)
mig(mach/restartable.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/restartable.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/restartable.h NOVOUCHERS)
mig(mach/vm_map.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/vm_map.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/vm_map.h NOVOUCHERS)

mig(mach/servers/netname.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/netname.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/netname.h NOVOUCHERS)

# This gets both a server and a client
mig(mach/exc.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE COMPILE_SERVER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/exc_user.c SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/exc_server.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/exc_user.h SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/exc_server.h NOVOUCHERS)

# These internal headers need to be installed privately for us to build off of.
mig(mach/mach_port.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/mach_port.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/mach_port_internal.h NOVOUCHERS)
mig(mach/mach_vm.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/mach_vm.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/mach_vm_internal.h NOVOUCHERS)
mig(mach/task.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/task.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/task_internal.h NOVOUCHERS)
mig(mach/thread_act.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/thread_act.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/thread_act_internal.h NOVOUCHERS)
mig(mach/vm_map.defs INCLUDES_FROM_TARGET libsystem_kernel_static NODEFINES TRUE USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/vm_map.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/vm_map_internal.h NOVOUCHERS)

# MIG output from above
target_sources(libsystem_kernel_static PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/clock_priv.c
    ${CMAKE_CURRENT_BINARY_DIR}/clock_reply.c
    ${CMAKE_CURRENT_BINARY_DIR}/clock.c
    ${CMAKE_CURRENT_BINARY_DIR}/exc_user.c
    ${CMAKE_CURRENT_BINARY_DIR}/host_priv.c
    ${CMAKE_CURRENT_BINARY_DIR}/host_security.c
    ${CMAKE_CURRENT_BINARY_DIR}/lock_set.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_host.c
    ${CMAKE_CURRENT_BINARY_DIR}/notify.c
    ${CMAKE_CURRENT_BINARY_DIR}/processor_set.c
    ${CMAKE_CURRENT_BINARY_DIR}/processor.c
    ${CMAKE_CURRENT_BINARY_DIR}/restartable.c
    ${CMAKE_CURRENT_BINARY_DIR}/memory_entry.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_voucher.c
    ${CMAKE_CURRENT_BINARY_DIR}/netname.c

    # This is bad practice, but as long as nobody goes around including <mach/*.c> it will be okay.
    ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/mach_port.c
    ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/mach_vm.c
    ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/task.c
    ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/thread_act.c
    ${CMAKE_CURRENT_BINARY_DIR}/internal_include/mach/vm_map.c
)

# We need to add these as sources to ensure they're built before the c files below,
#   as they are generated dependencies. It's a bit messy, but it all works out just fine.
target_sources(libsystem_kernel_static PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/clock.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/clock_priv.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/clock_reply.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/exc.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/host_priv.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/host_security.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/lock_set.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_host.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_port.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_voucher.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_vm.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/memory_entry.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/processor.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/processor_set.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/task.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/thread_act.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/vm_map.h

    ${CMAKE_CURRENT_BINARY_DIR}/public_include/servers/netname.h
)

# Public headers are just headers
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/clock.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/clock_priv.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/clock_reply.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/exc.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/host_priv.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/host_security.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/lock_set.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_host.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_port.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_voucher.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_vm.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/memory_entry.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/processor.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/processor_set.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/task.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/thread_act.h
    ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/vm_map.h

    ${CMAKE_CURRENT_BINARY_DIR}/public_include/servers/netname.h
    PROPERTIES HEADER_FILE_ONLY TRUE
)

# Ensure public headers are built before corresponding source files
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/clock.c
    ${CMAKE_CURRENT_BINARY_DIR}/clock_priv.c
    ${CMAKE_CURRENT_BINARY_DIR}/clock_reply.c
    ${CMAKE_CURRENT_BINARY_DIR}/exc.c
    ${CMAKE_CURRENT_BINARY_DIR}/host_priv.c
    ${CMAKE_CURRENT_BINARY_DIR}/host_security.c
    ${CMAKE_CURRENT_BINARY_DIR}/lock_set.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_eventlink.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_host.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_port.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_voucher.c
    ${CMAKE_CURRENT_BINARY_DIR}/memory_entry.c
    ${CMAKE_CURRENT_BINARY_DIR}/processor.c
    ${CMAKE_CURRENT_BINARY_DIR}/processor_set.c
    ${CMAKE_CURRENT_BINARY_DIR}/task.c
    ${CMAKE_CURRENT_BINARY_DIR}/thread_act.c
    ${CMAKE_CURRENT_BINARY_DIR}/vm_map.c
    ${CMAKE_CURRENT_BINARY_DIR}/netname.c
    PROPERTIES OBJECT_DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/clock.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/clock_priv.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/clock_reply.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/exc.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/host_priv.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/host_security.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/lock_set.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_eventlink.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_host.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_port.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_voucher.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_vm.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/memory_entry.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/processor.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/processor_set.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/task.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/thread_act.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/vm_map.h;
        ${CMAKE_CURRENT_BINARY_DIR}/public_include/servers/netname.h
)

# This library is installed in 4 locations for inclusion in various other packages
install(TARGETS libsystem_kernel_static DESTINATION usr/local/lib/system COMPONENT DeveloperInternal)
install(TARGETS libsystem_kernel_static DESTINATION usr/local/lib/dyld COMPONENT DeveloperInternal)
install(TARGETS libsystem_kernel_static DESTINATION usr/local/lib/loaderd COMPONENT DeveloperInternal)
install(TARGETS libsystem_kernel_static DESTINATION usr/local/lib/dyld_stub COMPONENT DeveloperInternal)

# END STATIC LIBRARY

add_darwin_shared_library(libsystem_kernel INSTALL_NAME_DIR /usr/lib/system)
set_property(TARGET libsystem_kernel PROPERTY OUTPUT_NAME system_kernel)

target_sources(libsystem_kernel PRIVATE
    wrappers/string/index.c
    wrappers/string/memcpy.c
    wrappers/string/memset.c
    wrappers/string/strcmp.c
    wrappers/string/strcpy.c
    wrappers/string/strlcpy.c
    wrappers/string/strlen.c
    wrappers/string/strsep.c
    wrappers/_libkernel_init.c
    wrappers/_libc_funcptr.c
    wrappers/system-version-compat.c
)

target_include_directories(libsystem_kernel PUBLIC include ${CMAKE_CURRENT_BINARY_DIR}/public_include)
target_include_directories(libsystem_kernel PRIVATE wrappers/string)

# Copied from static target above
target_include_directories(libsystem_kernel PRIVATE mach mach/mach mach/servers custom wrappers)
target_include_directories(libsystem_kernel PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/internal_include)
target_link_libraries(libsystem_kernel PRIVATE libsystem_kernel_private_headers)

target_compile_options(libsystem_kernel PRIVATE -fdollars-in-identifiers -no-cpp-precomp -fno-common -fno-stack-protector -fno-stack-check -momit-leaf-frame-pointer -Wno-nullability-completeness -Wno-implicit-function-declaration)
target_compile_definitions(libsystem_kernel PRIVATE _FORTIFY_SOURCE=0 __DARWIN_VERS_1050=1 NO_SYSCALL_LEGACY SYSCALL_PRE1050)

target_link_libraries(libsystem_kernel PRIVATE libsystem_kernel_static)
target_link_libraries(libsystem_kernel PRIVATE CrashReporterClient)
target_link_libraries(libsystem_kernel PRIVATE libc_public_headers)
target_link_options(libsystem_kernel PRIVATE -umbrella System -all_load)

target_link_options(libsystem_kernel PRIVATE -umbrella System -all_load)

install(TARGETS libsystem_kernel DESTINATION usr/lib/system COMPONENT BaseSystem)

# END DYNAMIC LIBRARY

install(FILES wrappers/gethostuuid.h DESTINATION usr/include COMPONENT DeveloperTools)
install(FILES wrappers/spawn/spawn.h DESTINATION usr/include COMPONENT DeveloperTools)
install(FILES wrappers/libproc/libproc.h DESTINATION usr/include COMPONENT DeveloperTools)

install(FILES wrappers/gethostuuid_private.h DESTINATION usr/local/include COMPONENT DeveloperInternal)
install(FILES wrappers/_libkernel_init.h DESTINATION usr/local/include COMPONENT DeveloperInternal)
install(FILES wrappers/spawn/spawn_private.h DESTINATION usr/local/include COMPONENT DeveloperInternal)
install(FILES wrappers/libproc/libproc_internal.h DESTINATION usr/local/include COMPONENT DeveloperInternal)

install(FILES os/proc.h DESTINATION usr/include COMPONENT DeveloperTools)

install(FILES os/thread_self_restrict.h DESTINATION usr/local/include/os COMPONENT DeveloperInternal)
install(FILES os/tsd.h DESTINATION usr/local/include/os COMPONENT DeveloperInternal)

install(FILES mach/mach/mach.h DESTINATION usr/include/mach COMPONENT DeveloperTools)
install(FILES mach/mach/mach_error.h DESTINATION usr/include/mach COMPONENT DeveloperTools)
install(FILES mach/mach/mach_init.h DESTINATION usr/include/mach COMPONENT DeveloperTools)
install(FILES mach/mach/mach_interface.h DESTINATION usr/include/mach COMPONENT DeveloperTools)
install(FILES mach/mach/mach_right.h DESTINATION usr/include/mach COMPONENT DeveloperTools)
install(FILES mach/mach/port_obj.h DESTINATION usr/include/mach COMPONENT DeveloperTools)
install(FILES mach/mach/sync.h DESTINATION usr/include/mach COMPONENT DeveloperTools)
install(FILES mach/mach/vm_task.h DESTINATION usr/include/mach COMPONENT DeveloperTools)
install(FILES mach/mach/vm_page_size.h DESTINATION usr/include/mach COMPONENT DeveloperTools)
install(FILES mach/mach/thread_state.h DESTINATION usr/include/mach COMPONENT DeveloperTools)

install(FILES mach/mach/port_descriptions.h DESTINATION usr/local/include/mach COMPONENT DeveloperInternal)
install(FILES mach/mach/mach_right_private.h DESTINATION usr/local/include/mach COMPONENT DeveloperInternal)
install(FILES mach/mach/mach_sync_ipc.h DESTINATION usr/local/include/mach COMPONENT DeveloperInternal)

install(FILES mach/servers/key_defs.h DESTINATION usr/include/servers COMPONENT DeveloperTools)
install(FILES mach/servers/ls_defs.h DESTINATION usr/include/servers COMPONENT DeveloperTools)
install(FILES mach/servers/netname_defs.h DESTINATION usr/include/servers COMPONENT DeveloperTools)
install(FILES mach/servers/nm_defs.h DESTINATION usr/include/servers COMPONENT DeveloperTools)

# Install MIG headers
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/public_include/servers DESTINATION usr/include COMPONENT DeveloperTools)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach DESTINATION usr/include COMPONENT DeveloperTools)

# END INSTALL HEADERS
