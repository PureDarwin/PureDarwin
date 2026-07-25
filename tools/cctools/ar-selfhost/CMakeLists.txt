set(_ar_selfhost_sources
    ../ar/append.c
    ../ar/ar.c
    ../ar/archive.c
    ../ar/contents.c
    ../ar/delete.c
    ../ar/extract.c
    ../ar/misc.c
    ../ar/move.c
    ../ar/print.c
    ../ar/replace.c
    pd_ar_compat.c
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libc/string/FreeBSD/strmode.c
)

add_darwin_executable(ar_selfhost MACOSX_VERSION_MIN 11.0)
get_target_property(_ar_selfhost_opts ar_selfhost COMPILE_OPTIONS)
if(_ar_selfhost_opts)
    list(REMOVE_ITEM _ar_selfhost_opts -nostdinc)
    set_target_properties(ar_selfhost PROPERTIES COMPILE_OPTIONS "${_ar_selfhost_opts}")
endif()
target_sources(ar_selfhost PRIVATE ${_ar_selfhost_sources})
target_include_directories(ar_selfhost PRIVATE ../ar ../include ../include/foreign ../libstuff)
target_compile_definitions(ar_selfhost PRIVATE
    __DARWIN_UNIX03=1
    _DARWIN_USE_64_BIT_INODE=1
)
target_compile_options(ar_selfhost PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK})
target_link_options(ar_selfhost PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,11.0,11.5
    -lSystem)
target_link_libraries(ar_selfhost PRIVATE libmacho_static libstuff_static)
add_dependencies(ar_selfhost libSystem_B_stub dyld)
set_property(TARGET ar_selfhost PROPERTY OUTPUT_NAME ar)
install(TARGETS ar_selfhost DESTINATION usr/bin COMPONENT BaseSystem)
