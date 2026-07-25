macro(cctools_selfhost_executable name)
    add_darwin_executable(${name}_selfhost MACOSX_VERSION_MIN 11.0)
    get_target_property(_selfhost_opts ${name}_selfhost COMPILE_OPTIONS)
    if(_selfhost_opts)
        list(REMOVE_ITEM _selfhost_opts -nostdinc)
        set_target_properties(${name}_selfhost PROPERTIES COMPILE_OPTIONS "${_selfhost_opts}")
    endif()
    target_sources(${name}_selfhost PRIVATE ../misc/${name}.c)
    target_include_directories(${name}_selfhost PRIVATE ../include ../include/foreign ../libstuff)
    target_compile_definitions(${name}_selfhost PRIVATE
        __DARWIN_UNIX03=1
        _DARWIN_USE_64_BIT_INODE=1
        PUREDARWIN_SELFHOST_CCTOOLS=1
    )
    target_compile_options(${name}_selfhost PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK})
    target_link_options(${name}_selfhost PRIVATE
        -nostdlib
        -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
        -Wl,-dylinker_install_name,/usr/lib/dyld
        -Wl,-platform_version,macos,11.0,11.5
        -lSystem)
    target_link_libraries(${name}_selfhost PRIVATE libmacho_static libstuff_static)
    add_dependencies(${name}_selfhost libSystem_B_stub dyld)
    set_property(TARGET ${name}_selfhost PROPERTY OUTPUT_NAME ${name})
    install(TARGETS ${name}_selfhost DESTINATION usr/bin COMPONENT BaseSystem)
endmacro()

cctools_selfhost_executable(lipo)
cctools_selfhost_executable(size)
cctools_selfhost_executable(strings)
cctools_selfhost_executable(checksyms)
cctools_selfhost_executable(nm)
cctools_selfhost_executable(libtool)
cctools_selfhost_executable(redo_prebinding)
cctools_selfhost_executable(seg_hack)
cctools_selfhost_executable(install_name_tool)
cctools_selfhost_executable(indr)
cctools_selfhost_executable(strip)
cctools_selfhost_executable(segedit)
cctools_selfhost_executable(pagestuff)
cctools_selfhost_executable(codesign_allocate)
cctools_selfhost_executable(bitcode_strip)
target_compile_definitions(bitcode_strip_selfhost PRIVATE -DALLOW_ARCHIVES)
cctools_selfhost_executable(ctf_insert)
cctools_selfhost_executable(check_dylib)
cctools_selfhost_executable(cmpdylib)
cctools_selfhost_executable(inout)

suppress_all_warnings(bitcode_strip_selfhost)
suppress_all_warnings(install_name_tool_selfhost)
suppress_all_warnings(strip_selfhost)

set(_cctools_selfhost_reallocf "${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libc/stdlib/FreeBSD/reallocf.c")

# Special cases: same source file as another tool, different behavior
# selected via a preprocessor define at compile time (mirrors misc/CMakeLists.txt).
macro(cctools_selfhost_executable_alias name source extra_define)
    add_darwin_executable(${name}_selfhost MACOSX_VERSION_MIN 11.0)
    get_target_property(_selfhost_opts ${name}_selfhost COMPILE_OPTIONS)
    if(_selfhost_opts)
        list(REMOVE_ITEM _selfhost_opts -nostdinc)
        set_target_properties(${name}_selfhost PROPERTIES COMPILE_OPTIONS "${_selfhost_opts}")
    endif()
    target_sources(${name}_selfhost PRIVATE ../misc/${source})
    target_include_directories(${name}_selfhost PRIVATE ../include ../include/foreign ../libstuff)
    target_compile_definitions(${name}_selfhost PRIVATE
        __DARWIN_UNIX03=1
        _DARWIN_USE_64_BIT_INODE=1
        PUREDARWIN_SELFHOST_CCTOOLS=1
        ${extra_define}
    )
    target_compile_options(${name}_selfhost PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK})
    target_link_options(${name}_selfhost PRIVATE
        -nostdlib
        -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
        -Wl,-dylinker_install_name,/usr/lib/dyld
        -Wl,-platform_version,macos,11.0,11.5
        -lSystem)
    target_link_libraries(${name}_selfhost PRIVATE libmacho_static libstuff_static)
    add_dependencies(${name}_selfhost libSystem_B_stub dyld)
    set_property(TARGET ${name}_selfhost PROPERTY OUTPUT_NAME ${name})
    install(TARGETS ${name}_selfhost DESTINATION usr/bin COMPONENT BaseSystem)
endmacro()

cctools_selfhost_executable_alias(nmedit strip.c -DNMEDIT)
cctools_selfhost_executable_alias(ranlib libtool.c -DRANLIB)

target_sources(libtool_selfhost PRIVATE "${_cctools_selfhost_reallocf}")
target_sources(ranlib_selfhost PRIVATE "${_cctools_selfhost_reallocf}")
