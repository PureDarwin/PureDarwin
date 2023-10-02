add_kext_bundle(pthread.kext INFO_PLIST pthread-Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kec.pthread BUNDLE_VERSION 1.0.0d1
    MAIN_FUNCTION pthread_start ANTIMAIN_FUNCTION pthread_stop)
set_property(TARGET pthread.kext PROPERTY OUTPUT_NAME pthread)

target_sources(pthread.kext PRIVATE
    kern_init.c
    kern_support.c
    kern_synch.c
)

target_compile_definitions(pthread.kext PRIVATE XNU_KERNEL_PRIVATE MACH_KERNEL_PRIVATE ABSOLUTETIME_SCALAR_TYPE NEEDS_SCHED_CALL_T __PTHREAD_EXPOSE_INTERNALS__)
target_link_libraries(pthread.kext PRIVATE pthread_common_private_headers pthread_common_headers xnu_kernel_private_headers xnu_private_headers)
target_compile_options(pthread.kext PRIVATE -Wno-sign-conversion -Wno-int-conversion -Wno-implicit-function-declaration -Wno-incompatible-library-redeclaration)

install(TARGETS pthread.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
