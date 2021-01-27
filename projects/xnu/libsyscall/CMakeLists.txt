add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/syscalls.a
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/sys
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/xcodescripts/create-syscalls.pl
        ${CMAKE_CURRENT_SOURCE_DIR}/../bsd/kern/syscalls.master
        ${CMAKE_CURRENT_SOURCE_DIR}/custom
        ${CMAKE_CURRENT_SOURCE_DIR}/Platforms
        MacOSX
        ${CMAKE_CURRENT_BINARY_DIR}/sys
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/xcodescripts/compile-syscalls.pl
        ${CMAKE_CURRENT_BINARY_DIR}/sys/stubs.list
        ${CMAKE_CURRENT_BINARY_DIR}/syscalls.a
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/xcodescripts/create-syscalls.pl
        ${CMAKE_CURRENT_SOURCE_DIR}/xcodescripts/compile-syscalls.pl
        ${CMAKE_CURRENT_SOURCE_DIR}/../bsd/kern/syscalls.master
    COMMENT "Compile system call stubs" VERBATIM
)

add_darwin_shared_library(libsystem_kernel)
target_link_libraries(libsystem_kernel PUBLIC AvailabilityHeaders)
target_link_libraries(libsystem_kernel PUBLIC xnu_headers libsyscall_headers)
target_link_libraries(libsystem_kernel PRIVATE xnu_private_headers libsyscall_private_headers)
target_sources(libsystem_kernel PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/syscalls.a)

target_sources(libsystem_kernel PRIVATE
    os/alloc_once.c
    os/log_data.c

    wrappers/__commpage_gettimeofday.c
    wrappers/__get_cpu_capabilities.s
    wrappers/_libc_funcptr.c
    wrappers/_libkernel_init.c
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
    wrappers/open_dprotected_np.c
    wrappers/open-base.c
    wrappers/persona.c
    wrappers/pid_shutdown_networking.c
    wrappers/proc.c
    wrappers/quota_obsolete.c
    wrappers/reboot.c
    wrappers/remove-counter.c
    wrappers/rename.c
    wrappers/renameat.c
    wrappers/renamex.c
    wrappers/rmdir.c
    wrappers/select-base.c
    wrappers/sfi.c
    wrappers/sigsuspend-base.c
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
)

add_library(libsyscall_headers INTERFACE)
target_include_directories(libsystem_kernel INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/public_headers)
target_include_directories(libsystem_kernel INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/public_headers)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/public_headers)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/public_headers/mach)

target_include_directories(libsystem_kernel PRIVATE INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/private_headers)
target_include_directories(libsystem_kernel PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/private_headers)
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

set(PUBLIC_DEFS
    clock clock_priv exc host_priv host_security lock_set
    mach_eventlink mach_host mach_port memory_entry processor processor_set
    task thread_act vm_map
)
foreach(name IN LISTS PUBLIC_DEFS)
    process_mig_source(public_headers/mach/${name}.defs NOVOUCHERS
        CLIENT_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_headers/mach/${name}.h
        TARGET libsystem_kernel
    )

    target_sources(libsystem_kernel PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/public_headers/mach/${name}.h)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/public_headers/mach/${name}.h DESTINATION usr/include/mach COMPONENT DeveloperTools)
endforeach()
