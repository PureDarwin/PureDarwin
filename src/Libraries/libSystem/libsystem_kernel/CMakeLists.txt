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

add_darwin_static_library(libsystem_kernel_static)
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
    mach/thread_act.c

    wrappers/__commpage_gettimeofday.c
    wrappers/__get_cpu_capabilities.s
    wrappers/_libc_funcptr.c
    wrappers/_libkernel_init.c
    wrappers/clonefile.c
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
    wrappers/mach_boottime.c
    wrappers/mach_continuous_time.c
    wrappers/mach_get_times.c
    wrappers/mach_timebase_info.c
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
    wrappers/system-version-compat.c
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
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/public_include/server)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/private_include/mach)

target_include_directories(libsystem_kernel_static PUBLIC include ${CMAKE_CURRENT_BINARY_DIR}/public_include)
target_include_directories(libsystem_kernel_static PRIVATE bootstrap_include mach wrappers ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach ${CMAKE_CURRENT_BINARY_DIR}/public_include/servers ${CMAKE_CURRENT_BINARY_DIR}/private_include ${CMAKE_CURRENT_BINARY_DIR}/private_include/mach)
target_compile_options(libsystem_kernel_static PRIVATE -fdollars-in-identifiers -no-cpp-precomp -fno-common -fno-stack-protector -fno-stack-check -momit-leaf-frame-pointer -Wno-nullability-completeness -Wno-implicit-function-declaration)
target_compile_definitions(libsystem_kernel_static PRIVATE _FORTIFY_SOURCE=0 __DARWIN_VERS_1050=1 NO_SYSCALL_LEGACY SYSCALL_PRE1050)
target_link_libraries(libsystem_kernel_static PRIVATE xnu_private_headers libsystem_kernel_private_headers libsyscall_traps)
target_link_libraries(libsystem_kernel_static PUBLIC xnu_headers pthread_common_headers AvailabilityHeaders)

mig(mach/clock_priv.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/clock_priv.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/private_include/mach/clock_priv.h)
mig(mach/clock_reply.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/clock_reply.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/clock_reply_user.h)
mig(mach/clock.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/clock.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/clock.h NOVOUCHERS)
mig(mach/exc.defs INCLUDES_FROM_TARGET libsystem_kernel_static COMPILE_SERVER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/exc_user.c SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/exc_server.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/exc.h SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/exc_server.h NOVOUCHERS)
mig(mach/host_priv.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/host_priv.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/host_priv.h NOVOUCHERS)
mig(mach/host_security.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/host_security.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/host_security.h NOVOUCHERS)
mig(mach/lock_set.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/lock_set.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/lock_set.h NOVOUCHERS)
mig(mach/mach_host.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_host.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_host.h NOVOUCHERS)
mig(mach/mach_port.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_port.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_port.h NOVOUCHERS)
mig(mach/mach_vm.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_vm.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_vm.h NOVOUCHERS)
mig(mach/notify.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/notify.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/notify_user.h)
mig(mach/processor_set.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/processor_set.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/processor_set.h NOVOUCHERS)
mig(mach/processor.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/processor.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/processor.h NOVOUCHERS)
mig(mach/task.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/task.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/task.h)
mig(mach/thread_act.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/thread_act.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/thread_act.h NOVOUCHERS)
mig(mach/vm_map.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/vm_map.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/vm_map.h NOVOUCHERS)
mig(mach/restartable.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/restartable.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/restartable_user.h)
mig(mach/memory_entry.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/memory_entry.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/memory_entry.h NOVOUCHERS)
mig(mach/mach_eventlink.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_eventlink.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mach_eventlink_user.h)
mig(mach/mach_voucher.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_voucher.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mach_voucher_user.h)
mig(include/servers/netname.defs INCLUDES_FROM_TARGET libsystem_kernel_static USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/netname.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/servers/netname.h NOVOUCHERS)

target_sources(libsystem_kernel_static PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/clock_priv.c
    ${CMAKE_CURRENT_BINARY_DIR}/clock_reply.c
    ${CMAKE_CURRENT_BINARY_DIR}/clock.c
    ${CMAKE_CURRENT_BINARY_DIR}/exc_user.c
    ${CMAKE_CURRENT_BINARY_DIR}/host_priv.c
    ${CMAKE_CURRENT_BINARY_DIR}/host_security.c
    ${CMAKE_CURRENT_BINARY_DIR}/lock_set.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_host.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_port.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_vm.c
    ${CMAKE_CURRENT_BINARY_DIR}/notify.c
    ${CMAKE_CURRENT_BINARY_DIR}/processor_set.c
    ${CMAKE_CURRENT_BINARY_DIR}/processor.c
    ${CMAKE_CURRENT_BINARY_DIR}/task.c
    ${CMAKE_CURRENT_BINARY_DIR}/thread_act.c
    ${CMAKE_CURRENT_BINARY_DIR}/vm_map.c
    ${CMAKE_CURRENT_BINARY_DIR}/restartable.c
    ${CMAKE_CURRENT_BINARY_DIR}/memory_entry.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_eventlink.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_voucher.c
    ${CMAKE_CURRENT_BINARY_DIR}/netname.c
)


install(FILES include/servers/ls_defs.h DESTINATION /usr/include/servers COMPONENT DeveloperTools)
install(FILES include/servers/netname_defs.h DESTINATION /usr/include/servers COMPONENT DeveloperTools)
install(FILES include/servers/nm_defs.h DESTINATION /usr/include/servers COMPONENT DeveloperTools)
install(FILES include/mach/mach.h DESTINATION /usr/include/mach COMPONENT DeveloperTools)
install(FILES include/mach/mach_error.h DESTINATION /usr/include/mach COMPONENT DeveloperTools)
install(FILES include/mach/mach_init.h DESTINATION /usr/include/mach COMPONENT DeveloperTools)
install(FILES include/mach/mach_interface.h DESTINATION /usr/include/mach COMPONENT DeveloperTools)
install(FILES include/mach/mach_right.h DESTINATION /usr/include/mach COMPONENT DeveloperTools)
install(FILES include/mach/port_obj.h DESTINATION /usr/include/mach COMPONENT DeveloperTools)
install(FILES include/mach/sync.h DESTINATION /usr/include/mach COMPONENT DeveloperTools)
install(FILES include/mach/vm_task.h DESTINATION /usr/include/mach COMPONENT DeveloperTools)
install(FILES include/mach/vm_page_size.h DESTINATION /usr/include/mach COMPONENT DeveloperTools)
install(FILES include/mach/thread_state.h DESTINATION /usr/include/mach COMPONENT DeveloperTools)


add_darwin_shared_library(libsystem_kernel NO_STANDARD_LIBRARIES INSTALL_NAME_DIR /usr/lib/system)
target_sources(libsystem_kernel PRIVATE
    wrappers/string/index.c
    wrappers/string/memcpy.c
    wrappers/string/memset.c
    wrappers/string/strcmp.c
    wrappers/string/strcpy.c
    wrappers/string/strlcpy.c
    wrappers/string/strlen.c
    wrappers/string/strsep.c
)
target_include_directories(libsystem_kernel PUBLIC include)
target_include_directories(libsystem_kernel PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/public_include)
target_include_directories(libsystem_kernel PRIVATE bootstrap_include wrappers/string)
target_compile_definitions(libsystem_kernel PRIVATE _FORTIFY_SOURCE=0)
target_link_libraries(libsystem_kernel PRIVATE libsystem_kernel_static)
target_link_options(libsystem_kernel PRIVATE -umbrella System -all_load)
install(TARGETS libsystem_kernel DESTINATION /usr/lib/system COMPONENT BaseSystem)

add_library(libsystem_kernel_private_headers INTERFACE)
target_include_directories(libsystem_kernel_private_headers INTERFACE private_include)
target_include_directories(libsystem_kernel_private_headers INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/private_include)

add_library(libsystem_kernel_bootstrap_headers INTERFACE)
target_include_directories(libsystem_kernel_bootstrap_headers INTERFACE bootstrap_include)
target_compile_options(libsystem_kernel INTERFACE -Wno-nullability-completeness)
