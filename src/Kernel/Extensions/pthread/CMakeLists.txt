add_kext_bundle(pthread.kext INFO_PLIST pthread-Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.kec.pthread BUNDLE_VERSION 1.0.0d1
    MAIN_FUNCTION pthread_start ANTIMAIN_FUNCTION pthread_stop)
set_property(TARGET pthread.kext PROPERTY OUTPUT_NAME pthread)

target_sources(pthread.kext PRIVATE
    kern_init.c
    kern_support.c
    kern_synch.c
)

# Keep generated Kernel.framework headers authoritative. They apply the same
# external-kext sanitization as Apple's internal SDK.
target_include_directories(pthread.kext BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/compat)

target_compile_definitions(pthread.kext PRIVATE
    XNU_KERNEL_PRIVATE MACH_KERNEL_PRIVATE
    CONFIG_SCHED_TIMESHARE_CORE=1 CONFIG_SCHED_CLUTCH=0 CONFIG_SCHED_EDGE=0 CONFIG_SCHED_SMT=1
    ABSOLUTETIME_SCALAR_TYPE NEEDS_SCHED_CALL_T __PTHREAD_EXPOSE_INTERNALS__ PRIVATE=1
    PD_FORCE_LOCAL_STDATOMIC)
target_compile_options(pthread.kext PRIVATE
    -include${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS/stdatomic.h
    -include${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS/pd_pthread_sched.h)
target_link_libraries(pthread.kext PRIVATE pthread_common_private_headers pthread_common_headers xnu_kernel_private_headers xnu_private_headers)
target_compile_options(pthread.kext PRIVATE -Wno-sign-conversion -Wno-int-conversion -Wno-implicit-function-declaration -Wno-incompatible-library-redeclaration)

install(TARGETS pthread.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
