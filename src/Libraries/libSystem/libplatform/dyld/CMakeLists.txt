add_darwin_static_library(libsystem_platform_dyld)

target_sources(libsystem_platform_dyld PRIVATE
      ../src/atomics/init.c
      ../src/atomics/common/MKGetTimeBaseInfo.c
      ../src/introspection/introspection.c
      ../src/os/alloc_once.c
      ../src/os/atomic.c
      ../src/os/lock.c
      ../src/os/semaphore.c
      ../src/setjmp/generic/setjmperr.c
      ../src/setjmp/generic/sigaction.c
      ../src/setjmp/generic/sigtramp.c
      ../src/simple/asl.c
      ../src/simple/getenv.c
      ../src/simple/string_io.c
      ../src/string/generic/bzero.c
      ../src/string/generic/ffsll.c
      ../src/string/generic/flsll.c
      ../src/string/generic/memccpy.c
      ../src/string/generic/memchr.c
      ../src/string/generic/memcmp.c
      ../src/string/generic/memmove.c
      ../src/string/generic/memset_pattern.c
      ../src/string/generic/strchr.c
      ../src/string/generic/strcmp.c
      ../src/string/generic/strcpy.c
      ../src/string/generic/strlcat.c
      ../src/string/generic/strlcpy.c
      ../src/string/generic/strlen.c
      ../src/string/generic/strncmp.c
      ../src/string/generic/strncpy.c
      ../src/string/generic/strnlen.c
      ../src/string/generic/strstr.c
      ../src/ucontext/generic/getmcontext.c
      ../src/ucontext/generic/makecontext.c
      ../src/ucontext/generic/setcontext.c
      ../src/ucontext/generic/swapcontext.c
      ../src/init.c

      ../src/atomics/x86_64/OSAtomic.s
      ../src/atomics/x86_64/OSAtomicFifo.c
      ../src/atomics/x86_64/pfz.s
      ../src/cachecontrol/x86_64/cache.s
      ../src/setjmp/x86_64/_setjmp.s
      ../src/setjmp/x86_64/_sigtramp.s
      ../src/setjmp/x86_64/setjmp.s
      ../src/ucontext/x86_64/_ctx_start.s
      ../src/ucontext/x86_64/_setcontext.s
      ../src/ucontext/x86_64/getcontext.s
)

target_include_directories(libsystem_platform_dyld PRIVATE ../src/os/resolver ../include)
target_compile_definitions(libsystem_platform_dyld PRIVATE VARIANT_STATIC=1 VARIANT_DYLD=1)
target_link_libraries(libsystem_platform_dyld INTERFACE libplatform_headers)
target_link_libraries(libsystem_platform_dyld PRIVATE libplatform_private_headers)
target_link_libraries(libsystem_platform_dyld PRIVATE
      libplatform_private_headers libsystem_kernel_bootstrap_headers libsystem_kernel_private_headers
      pthread_common_private_headers xnu_private_headers libsystem_kernel
)
target_link_libraries(libsystem_platform_dyld PUBLIC
      AvailabilityHeaders pthread_common_headers xnu_headers
)

target_compile_definitions(libsystem_platform_dyld PUBLIC TARGET_OS_OSX=1)
target_compile_options(libsystem_platform_dyld PRIVATE
      -fno-stack-check -fno-stack-protector -fno-common -momit-leaf-frame-pointer
)
target_compile_options(libsystem_platform_dyld PRIVATE -Wno-implicit-function-declaration)
target_compile_options(libsystem_platform_dyld PRIVATE -Wno-deprecated-declarations)
