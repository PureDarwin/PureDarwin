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
    ABSOLUTETIME_SCALAR_TYPE NEEDS_SCHED_CALL_T __PTHREAD_EXPOSE_INTERNALS__ PRIVATE=1
    PD_FORCE_LOCAL_STDATOMIC)

# These all gate fields of struct thread, so they MUST match the kernel this
# kext is loaded into or every thread_t offset past the first mismatch is wrong.
# The kernel gets them from config/MASTER.<arch> (SCHED_BASE) and
# osfmk/<arch>/proc_reg.h; the sanitized Kernel.framework headers the kext builds
# against pull in neither, so they are restated here per arch.
#
#   arm64 SCHED_BASE: config_sched_timeshare_core config_sched_deferred_ast
#                     config_clutch config_sched_sfi config_taskwatch
#                     config_preadopt_tg          (note: no config_sched_smt)
#   x86_64 SCHED_BASE: config_sched_timeshare_core config_sched_sfi
#                      config_sched_smt           (no clutch, no thread groups)
#
# CONFIG_SCHED_CLUTCH is derived in osfmk/kern/sched.h from CONFIG_CLUTCH on
# arm64; CONFIG_SCHED_EDGE stays off because virt is not __AMP__.
if(PUREDARWIN_ARM64 OR PUREDARWIN_ARM32)
    target_compile_definitions(pthread.kext PRIVATE
        CONFIG_SCHED_TIMESHARE_CORE=1
        CONFIG_SCHED_DEFERRED_AST=1
        CONFIG_CLUTCH=1
        CONFIG_SCHED_EDGE=0
        CONFIG_SCHED_SMT=0
        CONFIG_SCHED_SFI=1
        CONFIG_TASKWATCH=1
        CONFIG_PREADOPT_TG=1
        CONFIG_THREAD_GROUPS=1)
else()
    # Unchanged from before arm64 was wired up; x86 boots with these.
    target_compile_definitions(pthread.kext PRIVATE
        CONFIG_SCHED_TIMESHARE_CORE=1
        CONFIG_SCHED_CLUTCH=0
        CONFIG_SCHED_EDGE=0
        CONFIG_SCHED_SMT=1)
endif()
target_compile_options(pthread.kext PRIVATE
    -include${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS/stdatomic.h
    -include${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS/pd_pthread_sched.h)
target_link_libraries(pthread.kext PRIVATE pthread_common_private_headers pthread_common_headers xnu_kernel_private_headers xnu_private_headers)
target_compile_options(pthread.kext PRIVATE -Wno-sign-conversion -Wno-int-conversion -Wno-implicit-function-declaration -Wno-incompatible-library-redeclaration)

install(TARGETS pthread.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
