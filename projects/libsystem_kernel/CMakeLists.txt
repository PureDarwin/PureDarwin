add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/syscalls.a
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/sys
    COMMAND ${CMAKE_COMMAND} -E env ARCHS=x86_64
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/create-syscalls.pl
        ${CMAKE_CURRENT_SOURCE_DIR}/../xnu/bsd/kern/syscalls.master
        ${CMAKE_CURRENT_SOURCE_DIR}/custom ${CMAKE_CURRENT_SOURCE_DIR}/Platforms
        MacOSX ${CMAKE_CURRENT_BINARY_DIR}/sys
    COMMAND ${CMAKE_COMMAND} -E env ARCHS=x86_64
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/compile-syscalls.pl
        ${CMAKE_CURRENT_BINARY_DIR}/sys/stubs.list ${CMAKE_CURRENT_BINARY_DIR}/syscalls.a
        -I${PUREDARWIN_BINARY_DIR}/projects/xnu/xnu_header_install/usr/include
        -I${PUREDARWIN_BINARY_DIR}/projects/xnu/xnu_header_install/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders
        -I${PUREDARWIN_SOURCE_DIR}/projects/xnu/EXTERNAL_HEADERS
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/create-syscalls.pl
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/compile-syscalls.pl
        ${CMAKE_CURRENT_SOURCE_DIR}/../xnu/bsd/kern/syscalls.master
    COMMENT "Compile syscall stubs" VERBATIM
)

add_darwin_shared_library(libsystem_kernel NO_STANDARD_LIBRARIES INSTALL_NAME_DIR /usr/lib/system)
target_sources(libsystem_kernel PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/syscalls.a
    #mach/port_descriptions.c
    #wrappers/mach_bridge_remote_time.c
    # wrappers/clonefile.c
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/public_include/server)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach)

target_include_directories(libsystem_kernel PUBLIC include)
target_include_directories(libsystem_kernel PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/public_include)
target_include_directories(libsystem_kernel PRIVATE bootstrap_include mach ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach ${CMAKE_CURRENT_BINARY_DIR}/public_include/servers)
target_compile_definitions(libsystem_kernel PRIVATE _FORTIFY_SOURCE=0)
target_link_libraries(libsystem_kernel PUBLIC xnu_headers pthread_common_headers AvailabilityHeaders)
target_link_libraries(libsystem_kernel PRIVATE xnu_private_headers)
target_link_options(libsystem_kernel PRIVATE -umbrella System -all_load)

target_add_mig_sources(libsystem_kernel mach/clock_priv.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/clock_priv.c)
target_add_mig_sources(libsystem_kernel mach/clock_reply.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/clock_reply.c)
target_add_mig_sources(libsystem_kernel mach/clock.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/clock.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/clock.h NOVOUCHERS)
target_add_mig_sources(libsystem_kernel mach/exc.defs COMPILE_USER COMPILE_SERVER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/exc_user.c SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/exc_server.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/exc.h SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/exc_server.h NOVOUCHERS)
target_add_mig_sources(libsystem_kernel mach/host_priv.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/host_priv.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/host_priv.h NOVOUCHERS)
target_add_mig_sources(libsystem_kernel mach/host_security.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/host_security.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/host_security.h NOVOUCHERS)
target_add_mig_sources(libsystem_kernel mach/lock_set.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/lock_set.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/lock_set.h NOVOUCHERS)
target_add_mig_sources(libsystem_kernel mach/mach_host.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_host.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_host.h NOVOUCHERS)
target_add_mig_sources(libsystem_kernel mach/mach_port.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_port.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_port.h NOVOUCHERS)
target_add_mig_sources(libsystem_kernel mach/mach_vm.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_vm.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/mach_vm.h NOVOUCHERS)
target_add_mig_sources(libsystem_kernel mach/notify.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/notify.c)
target_add_mig_sources(libsystem_kernel mach/processor_set.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/processor_set.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/processor_set.h NOVOUCHERS)
target_add_mig_sources(libsystem_kernel mach/processor.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/processor.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/processor.h NOVOUCHERS)
target_add_mig_sources(libsystem_kernel mach/task.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/task.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/task.h)
target_add_mig_sources(libsystem_kernel mach/thread_act.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/thread_act.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/thread_act.h NOVOUCHERS)
target_add_mig_sources(libsystem_kernel mach/vm_map.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/vm_map.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/vm_map.h NOVOUCHERS)
target_add_mig_sources(libsystem_kernel mach/restartable.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/restartable.c)
target_add_mig_sources(libsystem_kernel mach/memory_entry.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/memory_entry.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/mach/memory_entry.h NOVOUCHERS)
target_add_mig_sources(libsystem_kernel mach/mach_eventlink.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_eventlink.c)
target_add_mig_sources(libsystem_kernel mach/mach_voucher.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_voucher.c)
target_add_mig_sources(libsystem_kernel include/servers/netname.defs COMPILE_USER USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/netname.c USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/public_include/servers/netname.h NOVOUCHERS)


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


add_library(libsystem_kernel_private_headers INTERFACE)
target_include_directories(libsystem_kernel_private_headers INTERFACE private_include)
