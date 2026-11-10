add_darwin_executable(dscacheutil MACOSX_VERSION_MIN 26.5)

get_target_property(_dscacheutil_opts dscacheutil COMPILE_OPTIONS)
if(_dscacheutil_opts)
    list(REMOVE_ITEM _dscacheutil_opts -nostdinc)
    set_target_properties(dscacheutil PROPERTIES COMPILE_OPTIONS "${_dscacheutil_opts}")
endif()

target_sources(dscacheutil PRIVATE
    dscacheutil.c
)

target_compile_options(dscacheutil PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -fno-builtin
    -fno-stack-protector
)

target_link_options(dscacheutil PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem
)

add_dependencies(dscacheutil libSystem_B_stub dyld)

install(TARGETS dscacheutil DESTINATION bin COMPONENT BaseSystem)
