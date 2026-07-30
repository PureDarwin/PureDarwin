# ext4.util and the filesystem bundle that points diskarbitrationd at it.

add_darwin_executable(ext4_util MACOSX_VERSION_MIN 11.0)
set_target_properties(ext4_util PROPERTIES OUTPUT_NAME ext4.util)

get_target_property(_ext4util_opts ext4_util COMPILE_OPTIONS)
if(_ext4util_opts)
    list(REMOVE_ITEM _ext4util_opts -nostdinc)
    set_target_properties(ext4_util PROPERTIES COMPILE_OPTIONS "${_ext4util_opts}")
endif()

target_sources(ext4_util PRIVATE ext4.util.c)

# Without this the SDK headers rename close()/pread() to their $UNIX2003
# variants, which libSystem does not export. Every other userspace target here
# sets it for the same reason.
target_compile_definitions(ext4_util PRIVATE __DARWIN_ONLY_UNIX_CONFORMANCE=1)

target_compile_options(ext4_util PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -fno-builtin
    -fno-stack-protector
)

target_include_directories(ext4_util PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/bsd            # sys/loadable_fs.h
)

target_link_options(ext4_util PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,11.0,11.5
    -lSystem
)

add_dependencies(ext4_util libSystem_B_stub dyld)

# The bundle layout diskarbitrationd walks: it scans
# /System/Library/Filesystems for *.fs, reads Contents/Info.plist, and resolves
# FSProbeExecutable as a bundle resource.
install(TARGETS ext4_util
    DESTINATION System/Library/Filesystems/ext4.fs/Contents/Resources
    COMPONENT BaseSystem)
install(FILES ext4.fs/Contents/Info.plist
    DESTINATION System/Library/Filesystems/ext4.fs/Contents
    COMPONENT BaseSystem)
