# wifiutil: scan, join and inspect an 802.11 interface through SIOCGA80211/SIOCSA80211
add_darwin_executable(wifiutil MACOSX_VERSION_MIN 26.5)

get_target_property(_wifiutil_opts wifiutil COMPILE_OPTIONS)
if(_wifiutil_opts)
    list(REMOVE_ITEM _wifiutil_opts -nostdinc)
    set_target_properties(wifiutil PROPERTIES COMPILE_OPTIONS "${_wifiutil_opts}")
endif()

target_sources(wifiutil PRIVATE wifiutil.c pdwifi.c)
if(PUREDARWIN_ARCH STREQUAL "arm64" OR PUREDARWIN_ARCH STREQUAL "riscv64")
    target_compile_definitions(wifiutil PRIVATE __DARWIN_ONLY_VERS_1050=1 __DARWIN_ONLY_64_BIT_INO_T=1)
endif()
target_compile_options(wifiutil PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -I${CMAKE_SOURCE_DIR}/src/Kernel/Extensions/IO80211Family
    -I${CMAKE_SOURCE_DIR}/src/Libraries/CommonCrypto/include
    -fno-stack-protector)
target_link_options(wifiutil PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem)
add_dependencies(wifiutil libSystem_B_stub dyld)
install(TARGETS wifiutil DESTINATION usr/sbin COMPONENT BaseSystem)
# joins a remembered network at boot, and again while the radio has none
install(FILES org.puredarwin.wifi-autojoin.plist
        DESTINATION System/Library/LaunchDaemons COMPONENT BaseSystem)
