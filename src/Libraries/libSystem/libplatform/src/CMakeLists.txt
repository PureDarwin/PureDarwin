add_darwin_shared_library(libsystem_platform INSTALL_NAME_DIR /usr/lib/system)
target_link_options(libsystem_platform PRIVATE -umbrella System -all_load)
install(TARGETS libsystem_platform DESTINATION /usr/lib/system COMPONENT BaseSystem)

target_link_libraries(libsystem_platform INTERFACE libplatform_headers)
target_link_libraries(libsystem_platform PRIVATE libsystem_kernel)
target_link_libraries(libsystem_platform PRIVATE libplatform_private_headers
     AvailabilityHeaders libsystem_kernel_bootstrap_headers
     libsystem_kernel_private_headers pthread_common_headers xnu_headers
)
add_dependencies(libsystem_platform libsystem_kernel_static)

target_sources(libsystem_platform PRIVATE
    init.c
)
