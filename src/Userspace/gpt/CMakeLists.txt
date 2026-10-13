# NetBSD's gpt (BSD licence), built the way NetBSD builds it as a host tool, with compat/ standing in for the host
add_darwin_executable(gpt MACOSX_VERSION_MIN 26.5)

get_target_property(_gpt_opts gpt COMPILE_OPTIONS)
if(_gpt_opts)
    list(REMOVE_ITEM _gpt_opts -nostdinc)
    set_target_properties(gpt PROPERTIES COMPILE_OPTIONS "${_gpt_opts}")
endif()

target_sources(gpt PRIVATE
    add.c biosboot.c create.c destroy.c gpt.c header.c label.c map.c main.c migrate.c
    recover.c remove.c resize.c resizedisk.c set.c show.c type.c unset.c gpt_uuid.c uuid.c
    compat/humanize_number.c compat/dehumanize_number.c compat/pd_gpt_compat.c)
target_include_directories(gpt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/compat)
target_compile_definitions(gpt PRIVATE HAVE_NBTOOL_CONFIG_H=1)
target_compile_options(gpt PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK}
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/compat/nbtool_config.h" -Wno-unused-parameter)
target_link_options(gpt PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem)
add_dependencies(gpt libSystem_B_stub dyld)

install(TARGETS gpt DESTINATION usr/sbin COMPONENT BaseSystem)
install(FILES gpt.8 DESTINATION usr/share/man/man8 COMPONENT BaseSystem)
