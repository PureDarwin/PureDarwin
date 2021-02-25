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
target_compile_definitions(libsystem_kernel PRIVATE _USE_FORTIFY_LEVEL=0)
target_compile_definitions(libsystem_kernel PRIVATE LIBSYSCALL_INTERFACE)
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

    wrappers/string/index.c
    wrappers/string/memcpy.c
    wrappers/string/memset.c
    wrappers/string/strcmp.c
    wrappers/string/strcpy.c
    wrappers/string/strlcpy.c
    wrappers/string/strlen.c
    wrappers/string/strsep.c

    wrappers/spawn/posix_spawn.c
    wrappers/libproc/libproc.c
    wrappers/libproc/proc_listpidspath.c
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
