add_darwin_static_library(libfirehose_kernel)
set_property(TARGET libfirehose_kernel PROPERTY OUTPUT_NAME "firehose_kernel")
target_sources(libfirehose_kernel PRIVATE
    src/firehose_buffer.c
)

target_include_directories(libfirehose_kernel PUBLIC include)
target_compile_definitions(libfirehose_kernel PRIVATE
    KERNEL=1 DISPATCH_USE_DTRACE=0 OS_ATOMIC_CONFIG_MEMORY_ORDER_DEPENDENCY=1
    OS_ATOMIC_CONFIG_MEMORY_ORDER_DEPENDENCY=1 OS_ATOMIC_CONFIG_STARVATION_FREE_ONLY=0)
target_compile_options(libfirehose_kernel PRIVATE -mkernel -Wno-packed -Wno-error=implicit-function-declaration -Wno-implicit-function-declaration)

# XNU proper is compiled with the per-SoC MACHINE_FLAGS; this library is not,
# so it takes the toolchain's default arm64 CPU, which is a much later core
# than an A10 and has the ARMv8.1 large-system-extension atomics. The result is
# an `ldadd` in __firehose_buffer_tracepoint_flush that a Hurricane core traps
# as an undefined instruction, panicking the moment os_log first flushes.
if(PUREDARWIN_ARM64_MACHINE_CONFIG STREQUAL "T8010")
    target_compile_options(libfirehose_kernel PRIVATE -mcpu=apple-a10)
endif()
# Kernel.framework first: this is a KERNEL=1 component, and xnu 12377's
# System.framework copy of firehose/tracepoint_private.h takes the userspace
# branch, which reads _COMM_PAGE_ATM_DIAGNOSTIC_CONFIG - kernel-private, and not
# exported by the userspace machine/cpu_capabilities.h.
target_link_libraries(libfirehose_kernel PRIVATE xnu_kernel_headers xnu_kernel_private_headers)
target_link_libraries(libfirehose_kernel PRIVATE AvailabilityHeaders xnu_private_headers)
target_link_libraries(libfirehose_kernel PRIVATE libplatform_headers libplatform_private_headers)
target_link_libraries(libfirehose_kernel PRIVATE pthread_common_headers pthread_common_private_headers)
