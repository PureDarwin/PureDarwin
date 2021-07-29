add_darwin_shared_library(libdyld NO_STANDARD_LIBRARIES INSTALL_NAME_DIR /usr/lib/system)
target_sources(libdyld PRIVATE
    src/dyld_process_info.cpp
    src/dyld_process_info_notify.cpp
    src/dyld_stub_binder.s
    src/dyldAPIsInLibSystem.cpp
    src/dyldLibSystemGlue.c
    src/dyldLock.cpp
    src/start_glue.s
    src/threadLocalVariables.c
    src/threadLocalHelpers.s

    dyld3/AllImages.cpp
    dyld3/APIs.cpp
    dyld3/APIs_macOS.cpp
    dyld3/ClosureFileSystemPhysical.cpp
    dyld3/ClosureBuilder.cpp
    dyld3/Closure.cpp
    dyld3/ClosureWriter.cpp
    dyld3/Diagnostics.cpp
    dyld3/Loading.cpp
    dyld3/Logging.cpp
    dyld3/MachOAnalyzerSet.cpp
    dyld3/MachOAnalyzer.cpp
    dyld3/MachOFile.cpp
    dyld3/MachOLoaded.cpp
    dyld3/PathOverrides.cpp
    dyld3/RootsChecker.cpp
    dyld3/Tracing.cpp
    dyld3/libdyldEntryVector.cpp
    dyld3/shared-cache/DyldSharedCache.cpp
)

target_link_libraries(libdyld PUBLIC dyld_headers)
target_link_libraries(libdyld PRIVATE libplatform_headers libplatform_private_headers libsystem_kernel xnu_private_headers libsystem_kernel_private_headers)
target_link_options(libdyld PRIVATE "LINKER:-no_inits" -umbrella System "LINKER:-unexported_symbol,__ZNSt3__18in_placeE")
set_property(TARGET libdyld PROPERTY CXX_STANDARD 11)

set_property(SOURCE ../dyld3/Loading.cpp APPEND PROPERTY COMPILE_FLAGS "-fvisibility=hidden")
