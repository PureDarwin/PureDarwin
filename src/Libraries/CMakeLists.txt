foreach(_puredarwin_library_dir
    AvailabilityVersions
    architecture
    CrashReporterClient
    CommonCrypto
    clang_bootstrap
    libcxxabi
    libSystem
    libresolv
    XPC
    dyld
    IOKit
    PDGOP
)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_puredarwin_library_dir}/CMakeLists.txt")
        add_subdirectory("${_puredarwin_library_dir}")
    endif()
endforeach()
