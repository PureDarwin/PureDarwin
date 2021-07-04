add_kext_bundle(pthread.kext INFO_PLIST pthread-Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kex.pthread BUNDLE_VERSION 1.0d1 MAIN_FUNCTION pthread_start ANTIMAIN_FUNCTION pthread_stop)
set_property(TARGET pthread.kext PROPERTY OUTPUT_NAME pthread)

target_sources(pthread.kext PRIVATE
    kern_init.c
    kern_support.c
    kern_synch.c
)

target_compile_definitions(pthread.kext PRIVATE XNU_KERNEL_PRIVATE MACH_KERNEL_PRIVATE ABSOLUTETIME_SCALAR_TYPE NEEDS_SCHED_CALL_T __PTHREAD_EXPOSE_INTERNALS__)
target_include_directories(pthread.kext PRIVATE .. ../include ../private $<TARGET_PROPERTY:xnu_kernel_private_headers,INTERFACE_INCLUDE_DIRECTORIES> $<TARGET_PROPERTY:xnu_private_headers,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_options(pthread.kext PRIVATE -Wno-sign-conversion -Wno-int-conversion -Wno-implicit-function-declaration)
add_kmod_info(pthread.kext IDENTIFIER org.puredarwin.kec.pthread VERSION 1.0.0.d1 MAIN_FUNCTION pthread_start ANTIMAIN_FUNCTION pthread_stop)

install(TARGETS pthread.kext DESTINATION /System/Library/Extensions COMPONENT BaseSystem)
