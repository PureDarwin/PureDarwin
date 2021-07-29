add_darwin_shared_library(libpthread NO_STANDARD_LIBRARIES INSTALL_NAME_DIR /usr/lib/system)
set_property(TARGET libpthread PROPERTY PREFIX lib)
set_property(TARGET libpthread PROPERTY OUTPUT_NAME system_pthread)

target_sources(libpthread PRIVATE
    pthread.c
    pthread_cancelable.c
    pthread_cond.c
    pthread_mutex.c
    pthread_rwlock.c
    pthread_cwd.c
    pthread_tsd.c
    pthread_atfork.c
    pthread_dependency.c
    pthread_asm.S
    qos.c
    variants/pthread_cancelable_cancel.c
)
target_include_directories(libpthread PUBLIC ../include)
target_include_directories(libpthread PRIVATE .. ../private)
target_compile_definitions(libpthread PRIVATE _FORTIFY_SOURCE=0 __POSIX_LIB__)
target_compile_options(libpthread PRIVATE -fno-stack-protector -fno-stack-check -fno-builtin)
target_link_options(libpthread PRIVATE "LINKER:-alias_file,pthread.aliases")
target_link_libraries(libpthread PRIVATE xnu_private_headers AvailabilityHeaders Libc_headers libplatform_private_headers libsystem_kernel libsystem_kernel_private_headers)

#add_firstpass_library(libpthread)


add_darwin_static_library(libpthread_dyld)
target_sources(libpthread_dyld PRIVATE
    pthread.c
    pthread_cancelable.c
    pthread_cond.c
    pthread_mutex.c
    pthread_rwlock.c
    pthread_cwd.c
    pthread_tsd.c
    pthread_atfork.c
    pthread_dependency.c
    qos.c
)
target_include_directories(libpthread_dyld PUBLIC ../include)
target_include_directories(libpthread_dyld PRIVATE .. ../private)
target_compile_definitions(libpthread_dyld PRIVATE VARIANT_DYLD)
target_compile_options(libpthread_dyld PRIVATE -fno-stack-protector -fno-stack-check -fno-builtin)
target_link_libraries(libpthread_dyld PRIVATE AvailabilityHeaders libplatform_private_headers libsystem_kernel xnu_private_headers xnu_kernel_private_headers)
