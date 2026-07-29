add_darwin_shared_library(libSystem_B_stub
    MACOSX_VERSION_MIN 11.0
    INSTALL_NAME /usr/lib/libSystem.B.dylib)
set_property(TARGET libSystem_B_stub PROPERTY OUTPUT_NAME "System.B")
add_custom_command(TARGET libSystem_B_stub POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
        "$<TARGET_FILE_NAME:libSystem_B_stub>"
        "$<TARGET_FILE_DIR:libSystem_B_stub>/libSystem.dylib"
    VERBATIM)

target_sources(libSystem_B_stub PRIVATE
    empty.c
    pd_libSystem_init.c
    pd_libSystem_compat.c
    pd_chkstk_darwin.s
    ${CMAKE_SOURCE_DIR}/src/Libraries/dyld/pd_libc_common_storage.c
    ${CMAKE_SOURCE_DIR}/src/Libraries/XPC/launchd/pd_sandbox_check.c
    # mach-o getsection helpers (getsectiondata/getsegmentdata/... ) that
    # libobjc references; libmacho isn't otherwise compiled into PD libSystem.
    ${CMAKE_SOURCE_DIR}/tools/cctools/libmacho/getsecbyname.c)

target_link_options(libSystem_B_stub PRIVATE
    -compatibility_version 1.0.0
    -current_version 1292.100.5)

get_target_property(_libsystem_b_copts libSystem_B_stub COMPILE_OPTIONS)
if(_libsystem_b_copts)
    list(REMOVE_ITEM _libsystem_b_copts -nostdinc)
    set_target_properties(libSystem_B_stub PROPERTIES COMPILE_OPTIONS "${_libsystem_b_copts}")
endif()
target_compile_options(libSystem_B_stub PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK})
target_link_options(libSystem_B_stub PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK})
target_include_directories(libSystem_B_stub PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Libraries/XPC/launchd/System-compat)

set(LIBSYSTEM_EXPORTS ${CMAKE_CURRENT_SOURCE_DIR}/libSystem.exports)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${LIBSYSTEM_EXPORTS})
file(STRINGS ${LIBSYSTEM_EXPORTS} _libsystem_export_lines)
set(_libsystem_exports_for_link ${LIBSYSTEM_EXPORTS})
if(PUREDARWIN_ARM64)
    # Drop only the legacy x86 ABI suffixes. Modern ARM64 APIs still use
    # Darwin extension/versioned names such as $DARWIN_EXTSN and $V2.
    list(FILTER _libsystem_export_lines EXCLUDE REGEX "\\$(UNIX2003|INODE64|1050)$")
    list(FILTER _libsystem_export_lines EXCLUDE REGEX "__ZNSt3__1plIc.*ERKS9_PKS6_")
    # The SDK headers use the unsuffixed ARM64 spellings for these syscalls,
    # while the shared export file only lists their x86 ABI variants.
    list(APPEND _libsystem_export_lines
        _select _fstat _fstatat _fstatfs _getfsstat _getmntinfo
        _lstat _stat _statfs _readdir _glob _globfree)
    set(_libsystem_exports_for_link ${CMAKE_CURRENT_BINARY_DIR}/libSystem.arm64.exports)
    file(WRITE ${_libsystem_exports_for_link} "")
    foreach(_line ${_libsystem_export_lines})
        file(APPEND ${_libsystem_exports_for_link} "${_line}\n")
    endforeach()
endif()
set(_libsystem_u_flags)
foreach(_line ${_libsystem_export_lines})
    string(STRIP "${_line}" _line)
    if(_line AND NOT _line MATCHES "^#")
        list(APPEND _libsystem_u_flags "-Wl,-u,${_line}")
    endif()
endforeach()
add_dependencies(libSystem_B_stub
    libsystem_kernel_static libplatform_static pthread_full_static
    libc_static libc_internal_static libmalloc_static
    corecrypto_static CrashReporterClient libresolv_static
    libdispatch_static openlibm_static XPC_notify_client_static XPC_notifyd_mig_static
    mDNSResponder_dnssd_static XPC_libinfo_static syslog_asl_static
    libsystem_trace_static libdarwin_static libSystemConfiguration_static
    XPC_libxpc_static XPC_libnv_static XPC_launchd_mig_static)
target_link_options(libSystem_B_stub PRIVATE
    -Wl,-force_load,$<TARGET_FILE:XPC_notify_client_static>
    # libdns_sd is part of libSystem on Darwin (libsystem_dnssd.dylib).
    -Wl,-force_load,$<TARGET_FILE:mDNSResponder_dnssd_static>
    # libinfo is part of libSystem on Darwin: it provides the real getpwnam/
    # getgrnam/getservbyname/gethostbyname/getaddrinfo/getfsent, which were
    # previously hand-stubbed in libc because this archive was never linked.
    -Wl,-force_load,$<TARGET_FILE:XPC_libinfo_static>
    # libsystem_asl.dylib is part of libSystem on Darwin; asl(3) + syslog(3).
    -Wl,-force_load,$<TARGET_FILE:syslog_asl_static>
    # libsystem_trace.dylib (os_log/os_signpost) is part of libSystem on Darwin;
    # libsystem_asl above calls os_log_with_args() from it.
    -Wl,-force_load,$<TARGET_FILE:libsystem_trace_static>
    # libdarwin (os_variant_*, os_boot_mode_*, the os/bsd.h + os/stdlib.h
    # helpers) is part of libSystem on Darwin.
    -Wl,-force_load,$<TARGET_FILE:libdarwin_static>
    # libSystemConfiguration.a is part of libSystem on Darwin: the dnsinfo
    # client (dns_configuration_copy) that libresolv and mDNSResponder use to
    # pick up configd's DNS configuration.
    -Wl,-force_load,$<TARGET_FILE:libSystemConfiguration_static>
    -Wl,-force_load,$<TARGET_FILE:XPC_libnv_static>
    -Wl,-force_load,$<TARGET_FILE:XPC_libxpc_static>
    $<TARGET_FILE:XPC_notifyd_mig_static>
    $<TARGET_FILE:XPC_launchd_mig_static>)
if(PUREDARWIN_USE_LD64_LLD)
    target_link_libraries(libSystem_B_stub PRIVATE
        $<TARGET_FILE:libplatform_static>
        $<TARGET_FILE:pthread_full_static>
        $<TARGET_FILE:libc_static>
        $<TARGET_FILE:libc_internal_static>
        $<TARGET_FILE:openlibm_static>
        $<TARGET_FILE:libmalloc_static>
        $<TARGET_FILE:libdispatch_static>
        $<TARGET_FILE:libsystem_kernel_static>
        ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/syscalls.a
        $<TARGET_FILE:corecrypto_static>
        $<TARGET_FILE:CrashReporterClient>
        $<TARGET_FILE:libresolv_static>)
else()
    target_link_libraries(libSystem_B_stub PRIVATE
        $<TARGET_FILE:libsystem_kernel_static>
        $<TARGET_FILE:libplatform_static>
        $<TARGET_FILE:pthread_full_static>
        $<TARGET_FILE:libc_static>
        $<TARGET_FILE:libc_internal_static>
        $<TARGET_FILE:openlibm_static>
        $<TARGET_FILE:libmalloc_static>
        $<TARGET_FILE:libdispatch_static>
        ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/syscalls.a
        $<TARGET_FILE:corecrypto_static>
        $<TARGET_FILE:CrashReporterClient>
        $<TARGET_FILE:libresolv_static>)
endif()
if(TARGET libcxxabi_static)
    add_dependencies(libSystem_B_stub libcxxabi_static_build)
    target_link_libraries(libSystem_B_stub PRIVATE $<TARGET_FILE:libcxxabi_static>)
endif()
if(TARGET libunwind_static)
    add_dependencies(libSystem_B_stub libunwind_static_build)
    target_link_libraries(libSystem_B_stub PRIVATE $<TARGET_FILE:libunwind_static>)
endif()
if(TARGET libcxx_static)
    add_dependencies(libSystem_B_stub libcxx_static_build)
    target_link_libraries(libSystem_B_stub PRIVATE $<TARGET_FILE:libcxx_static>)
endif()
if(TARGET commoncrypto_static)
    target_link_libraries(libSystem_B_stub PRIVATE $<TARGET_FILE:commoncrypto_static>)
endif()

# libdispatch's object.m (OS_OBJECT_USE_OBJC=1) references the real objc4
# runtime (objc_msgSend, class refs, etc.), resolved dynamically at runtime
# via dyld's flat-namespace lookup rather than a direct link dependency.
#
# libSystem must NOT gain a real (even weak) load-command dependency on
# libobjc: real Darwin has that edge the other way (objc depends on
# System), and dyld enforces that libSystem's own initializer runs before
# ANY other image's - if libobjc were a dependency of libSystem, its static
# initializers (objc4 has real ones - see __init_offsets) would run first
# during libSystem's own dependency walk and dyld panics with "initializer
# in image (...) that does not link with libSystem.dylib". libobjc instead
# gets loaded as an independent DYLD_INSERT_LIBRARIES root (see
# XPC/launchd/pd_launchd_boot.c and kern_exec.c's load_init_program_at_path,
# which seed that env var for pid 1 so every descendant process inherits
# it) - that puts it in the correct init order (libobjc depends on System,
# so System still initializes first) without any load-command edge here.
set(_libsystem_objc_undefined
    _class_createInstance
    __objc_atfork_child
    __objc_atfork_parent
    __objc_atfork_prepare
    _objc_autoreleasePoolPop
    _objc_autoreleasePoolPush
    _objc_begin_catch
    _OBJC_CLASS_$_NSObject
    _OBJC_METACLASS_$_NSObject
    _objc_destructInstance
    __objc_empty_cache
    _objc_end_catch
    __objc_init
    _objc_lookUpClass
    _objc_msgSend
    _objc_msgSendSuper2
    ___objc_personality_v0
    _objc_release
    _objc_retain
    _objc_terminate
    _object_getClassName)
set(_libsystem_objc_u_flags)
foreach(_sym ${_libsystem_objc_undefined})
    list(APPEND _libsystem_objc_u_flags "-Wl,-U,${_sym}")
endforeach()

set(LIBDISPATCH_ORDER
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libdispatch/xcodeconfig/libdispatch.order)
set_property(TARGET libSystem_B_stub APPEND PROPERTY LINK_DEPENDS ${LIBDISPATCH_ORDER})
target_link_options(libSystem_B_stub PRIVATE
    ${_libsystem_u_flags}
    ${_libsystem_objc_u_flags}
    -Wl,-order_file,${LIBDISPATCH_ORDER}
    -Wl,-dead_strip
    -Wl,-fixup_chains
    -Wl,-exported_symbols_list,${_libsystem_exports_for_link})
if(PUREDARWIN_USE_LD64_LLD)
    # ld64.lld diagnoses duplicate archive members before ordinary dead_strip
    # can discard them; Apple's ld64 accepts this link. Keep the compatibility
    # switch scoped to the Nix/LLVM linker path.
    target_link_options(libSystem_B_stub PRIVATE -Wl,--dead-strip-duplicates)
endif()
set_property(TARGET libSystem_B_stub APPEND PROPERTY LINK_DEPENDS ${LIBSYSTEM_EXPORTS})

add_dependencies(libSystem_B_stub libdyld)
target_link_libraries(libSystem_B_stub PRIVATE $<TARGET_FILE:libdyld>)
target_link_options(libSystem_B_stub PRIVATE
    -Wl,-reexport_library,$<TARGET_FILE:libdyld>)

install(TARGETS libSystem_B_stub LIBRARY DESTINATION usr/lib COMPONENT BaseSystem)
install_symlink(usr/lib/libSystem.dylib libSystem.B.dylib COMPONENT BaseSystem)
install_symlink(usr/lib/libc.dylib libSystem.B.dylib COMPONENT BaseSystem)
install_symlink(usr/lib/libm.dylib libSystem.B.dylib COMPONENT BaseSystem)
