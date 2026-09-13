# Real x86_64-apple-darwin build of iig (PureDarwin's from-scratch,
# kernel-side-subset IOKit interface generator - see
# https://github.com/PureDarwin/iig-tools), meant to eventually run ON
# PureDarwin itself, as opposed to the host-native iig built via the
# iig-tools flake input and used at kext-build time (cmake/iig.cmake).
#
# iig is plain C++17 (std::string/vector/map/set, no exceptions/RTTI) except
# for two tiny file-I/O helpers in main.cpp, which upstream (with PureDarwin's
# consent - it's their own tool) now use stdio instead of <fstream>/<sstream>
# specifically so this build doesn't need libc++'s iostream/locale/streambuf,
# which is not part of our target libcxx_static subset (see
# src/Libraries/libcxxabi/CMakeLists.txt).
#
# PUREDARWIN_IIG_SOURCE must point at an iig-tools checkout (mirrors
# src/Userspace/tcc's PUREDARWIN_TCC_SOURCE pattern).
if(NOT PUREDARWIN_IIG_SOURCE AND DEFINED ENV{PUREDARWIN_IIG_SOURCE})
    set(PUREDARWIN_IIG_SOURCE "$ENV{PUREDARWIN_IIG_SOURCE}")
endif()

if(NOT PUREDARWIN_IIG_SOURCE)
    message(WARNING "PUREDARWIN_IIG_SOURCE not set - skipping self-hosted iig target")
    return()
endif()

if(NOT EXISTS "${PUREDARWIN_IIG_SOURCE}/src/main.cpp")
    message(FATAL_ERROR "PUREDARWIN_IIG_SOURCE='${PUREDARWIN_IIG_SOURCE}' has no src/main.cpp")
endif()

# Toolchain/SDK selection, matching src/Libraries/libcxxabi/CMakeLists.txt.
get_filename_component(_iig_toolchain_bin "${CMAKE_AR}" DIRECTORY)
if(CMAKE_HOST_APPLE AND NOT CMAKE_CROSSCOMPILING)
    execute_process(
        COMMAND xcrun --sdk macosx --show-sdk-path
        OUTPUT_VARIABLE _iig_sdk
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _iig_sdk_result
    )
    if(NOT _iig_sdk_result EQUAL 0 OR NOT EXISTS "${_iig_sdk}/usr/include")
        set(_iig_sdk "${PUREDARWIN_MACOSX_SDK}")
    endif()
    find_program(_iig_real_clang NAMES clang REQUIRED)
    set(_iig_target_flags -arch "${PUREDARWIN_ARCH}")
else()
    get_filename_component(_iig_ar_name "${CMAKE_AR}" NAME)
    if(NOT _iig_ar_name MATCHES "^(.+)-ar$")
        message(FATAL_ERROR "Cannot derive the osxcross target triple from CMAKE_AR='${CMAKE_AR}'.")
    endif()
    set(_iig_triple "${CMAKE_MATCH_1}")
    if(DEFINED ENV{OSXCROSS_SDK})
        set(_iig_sdk "$ENV{OSXCROSS_SDK}")
    endif()
    if(NOT _iig_sdk)
        set(_iig_sdk "${PUREDARWIN_MACOSX_SDK}")
    endif()
    set(_iig_real_clang "${_iig_toolchain_bin}/${_iig_triple}-clang")
    if(NOT EXISTS "${_iig_real_clang}")
        message(FATAL_ERROR "Expected the osxcross clang wrapper at '${_iig_real_clang}'.")
    endif()
    set(_iig_target_flags)
endif()

set(_iig_libcxx "${CMAKE_SOURCE_DIR}/src/Libraries/libcxx")
set(_iig_libcxx_config "${CMAKE_SOURCE_DIR}/src/Libraries/libcxxabi/config")

set(_iig_cxx_flags
    ${_iig_target_flags}
    -isysroot ${_iig_sdk}
    -std=c++17 -nostdinc++ -fno-exceptions -fno-rtti -Os -DNDEBUG
    -Wall -Wno-unused-function
    -I ${PUREDARWIN_IIG_SOURCE}/include
)
# See the matching comment in src/Libraries/libcxxabi/CMakeLists.txt: header
# search order for -idirafter vs -I depends on which toolchain this is.
if((CMAKE_HOST_APPLE AND NOT CMAKE_CROSSCOMPILING) OR PUREDARWIN_NIX_TOOLCHAIN)
    list(APPEND _iig_cxx_flags -I ${_iig_libcxx_config} -I ${_iig_libcxx}/include)
else()
    list(APPEND _iig_cxx_flags -idirafter ${_iig_libcxx_config} -idirafter ${_iig_libcxx}/include)
endif()

set(_iig_srcs main parse codegen)
set(_iig_objs "")
foreach(s ${_iig_srcs})
    set(_obj "${CMAKE_CURRENT_BINARY_DIR}/${s}.o")
    add_custom_command(
        OUTPUT "${_obj}"
        COMMAND "${_iig_real_clang}" ${_iig_cxx_flags}
                -c "${PUREDARWIN_IIG_SOURCE}/src/${s}.cpp" -o "${_obj}"
        DEPENDS "${PUREDARWIN_IIG_SOURCE}/src/${s}.cpp"
        COMMENT "Building iig ${s}.cpp (raw clang, vendored libc++ headers)"
        VERBATIM
    )
    list(APPEND _iig_objs "${_obj}")
endforeach()

set(_iig_bin "${CMAKE_CURRENT_BINARY_DIR}/iig")
add_custom_command(
    OUTPUT "${_iig_bin}"
    COMMAND "${_iig_real_clang}" ${_iig_target_flags}
            -isysroot ${_iig_sdk}
            -nostdlib
            -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
            -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:$<TARGET_FILE:libdyld>
            -Wl,-dylinker_install_name,/usr/lib/dyld
            -Wl,-platform_version,macos,26.5,26.5
            -lSystem
            ${_iig_objs} -o "${_iig_bin}"
    DEPENDS ${_iig_objs} libSystem_B_stub dyld
    COMMENT "Linking iig (self-host)"
    VERBATIM
)
add_custom_target(iig_selfhost ALL DEPENDS "${_iig_bin}")
add_dependencies(iig_selfhost libSystem_B_stub dyld)

install(PROGRAMS "${_iig_bin}" DESTINATION usr/bin COMPONENT BaseSystem)
