foreach(_puredarwin_library_dir
    AvailabilityVersions
    architecture
    CrashReporterClient
    CommonCrypto
    clang_bootstrap
    libcxxabi
    libSystem
    libdarwin
    libsystem_trace
    mDNSResponder
    libresolv
    libSystemConfiguration
    XPC
    # after XPC: the MIG rule for asl_ipc.defs pulls its include path from
    # XPC_libxpc_static, so that target has to exist first.
    syslog
    # after XPC for the same reason: config.defs' MIG rule takes its include
    # path from the SystemConfiguration target, which needs libxpc's headers.
    SystemConfiguration
    DiskArbitration
    dyld
    IOKit
    PDGOP
)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_puredarwin_library_dir}/CMakeLists.txt")
        add_subdirectory("${_puredarwin_library_dir}")
    endif()
endforeach()
