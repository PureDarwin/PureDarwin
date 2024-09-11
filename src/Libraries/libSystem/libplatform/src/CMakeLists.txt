add_darwin_shared_library(libsystem_platform INSTALL_NAME_DIR /usr/lib/system)

target_sources(libsystem_platform PRIVATE
    atomics/init.c
    atomics/common/MKGetTimeBaseInfo.c
    introspection/introspection.c
    os/alloc_once.c
    os/atomic.c
    os/lock.c
    os/semaphore.c
    setjmp/generic/setjmperr.c
    setjmp/generic/sigaction.c
    setjmp/generic/sigtramp.c
    simple/asl.c
    simple/getenv.c
    simple/string_io.c
    string/generic/bzero.c
    string/generic/ffsll.c
    string/generic/flsll.c
    string/generic/memccpy.c
    string/generic/memchr.c
    string/generic/memcmp.c
    string/generic/memmove.c
    string/generic/memset_pattern.c
    string/generic/strchr.c
    string/generic/strcmp.c
    string/generic/strcpy.c
    string/generic/strlcat.c
    string/generic/strlcpy.c
    string/generic/strlen.c
    string/generic/strncmp.c
    string/generic/strncpy.c
    string/generic/strnlen.c
    string/generic/strstr.c
    ucontext/generic/getmcontext.c
    ucontext/generic/makecontext.c
    ucontext/generic/setcontext.c
    ucontext/generic/swapcontext.c
    init.c

    atomics/x86_64/OSAtomic.s
    atomics/x86_64/OSAtomicFifo.c
    atomics/x86_64/pfz.s
    cachecontrol/x86_64/cache.s
    setjmp/x86_64/_setjmp.s
    setjmp/x86_64/_sigtramp.s
    setjmp/x86_64/setjmp.s
    ucontext/x86_64/_ctx_start.s
    ucontext/x86_64/_setcontext.s
    ucontext/x86_64/getcontext.s
)

target_compile_definitions(libsystem_platform PUBLIC TARGET_OS_OSX=1)
target_include_directories(libsystem_platform PRIVATE os/resolver ../include)

target_compile_options(libsystem_platform PRIVATE
    -fno-stack-check -fno-stack-protector -fno-common -momit-leaf-frame-pointer
)
target_compile_options(libsystem_platform PRIVATE -Wno-implicit-function-declaration)
target_compile_options(libsystem_platform PRIVATE -Wno-deprecated-declarations)

target_link_libraries(libsystem_platform PRIVATE
    libplatform_private_headers libsystem_kernel_bootstrap_headers libsystem_kernel_private_headers
    pthread_common_private_headers xnu_private_headers
)

target_link_libraries(libsystem_platform PUBLIC AvailabilityHeaders pthread_common_headers xnu_headers)
target_link_libraries(libsystem_platform INTERFACE libplatform_headers)
target_link_libraries(libsystem_platform PRIVATE libplatform_private_headers)
target_link_libraries(libsystem_platform PRIVATE libsystem_kernel)
target_link_options(libsystem_platform PRIVATE -umbrella System)
target_link_options(libsystem_platform PRIVATE "LINKER:-bind_at_load")
target_link_options(libsystem_platform PRIVATE "LINKER:-alias_list,${CMAKE_CURRENT_SOURCE_DIR}/libplatform.aliases")

target_sources(libsystem_platform PRIVATE force_libplatform_to_build.c)
install(TARGETS libsystem_platform DESTINATION /usr/lib/system COMPONENT BaseSystem)
