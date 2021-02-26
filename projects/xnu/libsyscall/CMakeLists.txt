add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/syscalls.a
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/sys
    COMMAND ${CMAKE_COMMAND} -E env ARCHS=x86_64
        ${CMAKE_CURRENT_SOURCE_DIR}/xcodescripts/create-syscalls.pl
        ${CMAKE_CURRENT_SOURCE_DIR}/../bsd/kern/syscalls.master
        ${CMAKE_CURRENT_SOURCE_DIR}/custom
        ${CMAKE_CURRENT_SOURCE_DIR}/Platforms
        MacOSX
        ${CMAKE_CURRENT_BINARY_DIR}/sys
    COMMAND
        ${CMAKE_COMMAND} -E env
            DARWIN_LIBTOOL=$<TARGET_FILE:darwin_libtool>
            PUREDARWIN_SOURCE_DIR=${PUREDARWIN_SOURCE_DIR}
            SDKROOT=${CMAKE_CURRENT_BINARY_DIR}/../xnu_header_install ARCHS=x86_64
        ${CMAKE_CURRENT_SOURCE_DIR}/xcodescripts/compile-syscalls.pl
        ${CMAKE_CURRENT_BINARY_DIR}/sys/stubs.list
        ${CMAKE_CURRENT_BINARY_DIR}/syscalls.a
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/xcodescripts/create-syscalls.pl
        ${CMAKE_CURRENT_SOURCE_DIR}/xcodescripts/compile-syscalls.pl
        ${CMAKE_CURRENT_SOURCE_DIR}/../bsd/kern/syscalls.master
        xnu_headers
    COMMENT "Compile system call stubs" VERBATIM
)

add_library(libsystem_kernel SHARED)
add_dependencies(libsystem_kernel darwin_ld)
target_link_options(libsystem_kernel PRIVATE -fuse-ld=$<TARGET_FILE:darwin_ld>)
set_property(TARGET libsystem_kernel PROPERTY PREFIX "")
target_compile_options(libsystem_kernel PRIVATE -target x86_64-apple-darwin20)

target_link_libraries(libsystem_kernel PRIVATE xnu_private_headers) # Must be first
target_link_libraries(libsystem_kernel PUBLIC AvailabilityHeaders architecture_headers)
target_link_libraries(libsystem_kernel PUBLIC xnu_headers)
target_link_libraries(libsystem_kernel PRIVATE libsyscall_mig_headers)
target_link_libraries(libsystem_kernel PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/syscalls.a)
target_link_options(libsystem_kernel PRIVATE -nostdlib)
target_compile_options(libsystem_kernel PRIVATE -fno-stack-check -fno-stack-protector -fno-common)
target_compile_options(libsystem_kernel PRIVATE -Wno-nullability-completeness)
target_compile_options(libsystem_kernel PRIVATE -F ${CMAKE_CURRENT_BINARY_DIR}/../xnu_header_install/System/Library/Frameworks)
target_compile_definitions(libsystem_kernel PRIVATE _FORTIFY_SOURCE=0)
target_compile_definitions(libsystem_kernel PRIVATE LIBSYSCALL_INTERFACE)
target_compile_definitions(libsystem_kernel PRIVATE __DARWIN_UNIX03=1)
target_sources(libsystem_kernel PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/syscalls.a)

target_sources(libsystem_kernel PRIVATE
    custom/__fork.s
    custom/__getpid.s
    custom/__gettimeofday.s
    custom/__kdebug_trace_string.s
    custom/__lseek.s
    custom/__pipe.s
    custom/__ptrace.s
    custom/__sigaltstack.s
    custom/__sigreturn.s
    custom/__syscall.s
    custom/__thread_selfid.s
    custom/__thread_selfusage.s
    custom/__vfork.s
    custom/custom.s
    custom/dummy.c
    custom/errno.c
    mach/clock_sleep.c
    mach/error_codes.c
    mach/exc_catcher.c
    mach/exc_catcher_state.c
    mach/exc_catcher_state_identity.c
    mach/fprintf_stderr.c
    mach/host.c
    mach/mach_error.c
    mach/mach_error_string.c
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
    os/alloc_once.c
    os/log_data.c
    public_headers/mach/mach_traps.s
    wrappers/__commpage_gettimeofday.c
    wrappers/__get_cpu_capabilities.s
    wrappers/_libc_funcptr.c
    wrappers/_libkernel_init.c
    wrappers/cancelable/fcntl-base.c
    wrappers/cancelable/fcntl-cancel.c
    wrappers/cancelable/fcntl.c
    wrappers/cancelable/open.c
    wrappers/cancelable/pselect-darwinext-cancel.c
    wrappers/cancelable/pselect-darwinext.c
    wrappers/cancelable/select-cancel.c
    wrappers/cancelable/select.c
    wrappers/cancelable/sigsuspend-cancel.c
    wrappers/cancelable/sigsuspend.c
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
    wrappers/legacy/select-pre1050.c
    wrappers/legacy/select.c
    wrappers/legacy/sendmsg.c
    wrappers/legacy/sendto.c
    wrappers/legacy/setattrlist.c
    wrappers/legacy/sigsuspend.c
    wrappers/legacy/socketpair.c
    wrappers/libproc/libproc.c
    wrappers/libproc/proc_listpidspath.c
    wrappers/mach_absolute_time.s
    wrappers/mach_approximate_time.c
    wrappers/mach_approximate_time.s
    wrappers/mach_boottime.c
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
    wrappers/select-base.c
    wrappers/setpriority.c
    wrappers/sfi.c
    wrappers/sigsuspend-base.c
    wrappers/spawn/posix_spawn.c
    wrappers/stackshot.c
    wrappers/string/index.c
    wrappers/string/memcpy.c
    wrappers/string/memset.c
    wrappers/string/strcmp.c
    wrappers/string/strcpy.c
    wrappers/string/strlcpy.c
    wrappers/string/strlen.c
    wrappers/string/strsep.c
    wrappers/system-version-compat-support.c
    wrappers/system-version-compat.c
    wrappers/terminate_with_reason.c
    wrappers/thread_register_state.c
    wrappers/unix03/chmod.c
    wrappers/unix03/fchmod.c
    wrappers/unix03/getrlimit.c
    wrappers/unix03/mmap.c
    wrappers/unix03/munmap.c
    wrappers/unix03/setrlimit.c
    wrappers/unlink.c
    wrappers/unlinkat.c
    wrappers/utimensat.c
    wrappers/varargs_wrappers.s
    wrappers/work_interval.c
)

target_include_directories(libsystem_kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/public_headers)
target_include_directories(libsystem_kernel PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../xnu_header_install/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/public_headers)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/public_headers/mach)

target_include_directories(libsystem_kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private_headers)
target_include_directories(libsystem_kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/build_headers)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/private_headers)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/private_headers/mach)

install(
    FILES
        public_headers/servers/key_defs.h
        public_headers/servers/ls_defs.h
        public_headers/servers/netname_defs.h
        public_headers/servers/nm_defs.h
    DESTINATION usr/include/servers COMPONENT DeveloperTools
)
install(
    FILES
        public_headers/mach/mach.h
        public_headers/mach/mach_error.h
        public_headers/mach/mach_init.h
        public_headers/mach/mach_interface.h
        public_headers/mach/mach_right.h
        public_headers/mach/port_obj.h
        public_headers/mach/sync.h
        public_headers/mach/vm_task.h
        public_headers/mach/vm_page_size.h
        public_headers/mach/thread_state.h
    DESTINATION usr/include/mach COMPONENT DeveloperTools
)









add_library(libsyscall_mig_headers INTERFACE)
target_include_directories(libsyscall_mig_headers INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/public_headers)

process_mig_source(mach/servers/netname.defs
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_headers/mach/servers/netname.h
    TARGETS xnu_headers xnu_private_headers
)
install(FILES mach/server/netname.defs DESTINATION usr/include/mach COMPONENT DeveloperTools)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/public_headers/mach/netname.h DESTINATION usr/include/mach COMPONENT DeveloperTools)
target_sources(libsyscall_mig_headers PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/public_headers/mach/servers/netname.h)

set(public_sources
    clock clock_priv exc host_priv host_security lock_set
    mach_eventlink mach_host mach_port mach_voucher
    memory_entry processor processor_set task thread_act
    vm_map mach_vm
)
foreach(file IN LISTS public_sources)
    process_mig_source(mach/${file}.defs NOVOUCHERS
        SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_headers/mach/${file}.h
        COMPILE_DEFINITIONS LIBSYSCALL_INTERFACE
        TARGETS xnu_headers xnu_private_headers
    )
    install(FILES mach/${file}.defs DESTINATION usr/include/mach COMPONENT DeveloperTools)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/public_headers/mach/${file}.h DESTINATION usr/include/mach COMPONENT DeveloperTools)
    target_sources(libsyscall_mig_headers PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/public_headers/mach/${file}.h)
endforeach()

set(internal_sources mach_port mach_vm thread_act vm_map)
foreach(file IN LISTS internal_sources)
    process_mig_source(mach/${file}.defs NOVOUCHERS
        SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/private_headers/mach/${file}.h
        TARGETS xnu_headers xnu_private_headers
    )
    target_sources(libsyscall_mig_headers PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/private_headers/mach/${file}.h)
endforeach()
