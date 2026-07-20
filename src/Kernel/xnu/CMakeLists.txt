find_program(dsymutil_path NAMES dsymutil llvm-dsymutil REQUIRED)

execute_process(
    COMMAND make --version
    OUTPUT_VARIABLE _xnu_make_version_output
    ERROR_QUIET
)
set(XNU_MAKE_OUTPUT_SYNC_FLAG "")
if(_xnu_make_version_output MATCHES "GNU Make ([0-9]+)\\.")
    if(CMAKE_MATCH_1 GREATER_EQUAL 4)
        set(XNU_MAKE_OUTPUT_SYNC_FLAG "--output-sync=recurse")
    endif()
endif()

# XNU build arch. x86_64 installs the i386/ machine headers (machine/types.h
# includes "i386/types.h"); arm64 installs arm/ and needs a MACHINE_CONFIGS.
if(PUREDARWIN_ARM64)
    set(XNU_ARCH_CONFIGS ARM64)
    if(NOT DEFINED PUREDARWIN_ARM64_MACHINE_CONFIG)
        set(PUREDARWIN_ARM64_MACHINE_CONFIG BCM2837)
    endif()
    set(XNU_MACHINE_CONFIGS MACHINE_CONFIGS=${PUREDARWIN_ARM64_MACHINE_CONFIG})
else()
    set(XNU_ARCH_CONFIGS X86_64)
    set(XNU_MACHINE_CONFIGS "")
endif()

if(PUREDARWIN_NIX_TOOLCHAIN)
    # Real cctools ld64 (see the native-ld configure in build.nix), not
    # nixpkgs' ld64.lld - lld's Mach-O port doesn't implement "-kernel
    # -static" kernel-linking mode (drops -kernel silently, mis-parses
    # -platform_version) and never emits a correct entry-point load
    # command, so a kernel linked with it fails to boot ("failed to find
    # entry vmaddr"). Everything else still links fine with lld.
    set(XNU_LD_PATH "$ENV{NIX_NATIVE_LD_PATH}")
    get_filename_component(_nix_native_ld_dir "$ENV{NIX_NATIVE_LD_PATH}" DIRECTORY)
    if(EXISTS "${_nix_native_ld_dir}/strip")
        set(XNU_STRIP_PATH "${_nix_native_ld_dir}/strip")
    else()
        set(XNU_STRIP_PATH "$ENV{NIX_DARWIN_TOOLCHAIN_DIR}/x86_64-apple-darwin20.4-strip")
    endif()
    set(XNU_LIPO_PATH "$ENV{NIX_DARWIN_TOOLCHAIN_DIR}/x86_64-apple-darwin20.4-lipo")
    set(XNU_NM_PATH "$ENV{NIX_DARWIN_TOOLCHAIN_DIR}/x86_64-apple-darwin20.4-nm")
    set(XNU_HOST_CC_PATH "$ENV{NIX_HOST_CC_PATH}")
else()
    if(NOT CMAKE_HOST_APPLE)
        get_filename_component(_xnu_ar_name "${CMAKE_AR}" NAME)
        if(_xnu_ar_name MATCHES "^(.+)-ar$")
            set(_xnu_triple "${CMAKE_MATCH_1}")
            find_program(XNU_NATIVE_LD_EXECUTABLE NAMES "${_xnu_triple}-ld")
            find_program(XNU_NATIVE_STRIP_EXECUTABLE NAMES "${_xnu_triple}-strip")
            find_program(XNU_NATIVE_LIPO_EXECUTABLE NAMES "${_xnu_triple}-lipo")
            find_program(XNU_NATIVE_NM_EXECUTABLE NAMES "${_xnu_triple}-nm")
        endif()
    endif()
    if(XNU_NATIVE_LD_EXECUTABLE)
        set(XNU_LD_PATH "${XNU_NATIVE_LD_EXECUTABLE}")
    else()
        set(XNU_LD_PATH "$<TARGET_FILE:host_ld>")
    endif()
    if(XNU_NATIVE_STRIP_EXECUTABLE)
        set(XNU_STRIP_PATH "${XNU_NATIVE_STRIP_EXECUTABLE}")
    else()
        set(XNU_STRIP_PATH "$<TARGET_FILE:host_strip>")
    endif()
    if(XNU_NATIVE_LIPO_EXECUTABLE)
        set(XNU_LIPO_PATH "${XNU_NATIVE_LIPO_EXECUTABLE}")
    else()
        set(XNU_LIPO_PATH "$<TARGET_FILE:host_lipo>")
    endif()
    if(XNU_NATIVE_NM_EXECUTABLE)
        set(XNU_NM_PATH "${XNU_NATIVE_NM_EXECUTABLE}")
    else()
        set(XNU_NM_PATH "$<TARGET_FILE:host_nm>")
    endif()
    set(XNU_HOST_CC_PATH "")
endif()
if(XNU_HOST_CC_PATH)
    set(XNU_HOST_CC_ENV "HOST_CC=${XNU_HOST_CC_PATH}")
else()
    set(XNU_HOST_CC_ENV "")
endif()

set(PUREDARWIN_XNU_KERNEL_CONFIG "RELEASE" CACHE STRING
    "XNU kernel configuration to build: RELEASE, DEVELOPMENT, DEBUG, or KASAN")
set_property(CACHE PUREDARWIN_XNU_KERNEL_CONFIG PROPERTY STRINGS
    RELEASE DEVELOPMENT DEBUG KASAN)
string(TOUPPER "${PUREDARWIN_XNU_KERNEL_CONFIG}" PUREDARWIN_XNU_KERNEL_CONFIG)
if(NOT PUREDARWIN_XNU_KERNEL_CONFIG MATCHES "^(RELEASE|DEVELOPMENT|DEBUG|KASAN)$")
    message(FATAL_ERROR
        "Unsupported PUREDARWIN_XNU_KERNEL_CONFIG='${PUREDARWIN_XNU_KERNEL_CONFIG}'")
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install)
externalproject_add(xnu_headers.extproj
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_COMMAND "" # disable auto CMake configure
    BUILD_COMMAND
        ${CMAKE_COMMAND}
            -D XNU_SRC=${CMAKE_CURRENT_SOURCE_DIR}
            -D XNU_OBJ=<BINARY_DIR>
            -D CODESIGN_ALLOCATE_PATH=$<TARGET_FILE:host_codesign_allocate>
            -D CTFCONVERT_PATH=$<TARGET_FILE:host_ctfconvert>
            -D CTFMERGE_PATH=$<TARGET_FILE:host_ctfmerge>
            -D CTFINSERT_PATH=$<TARGET_FILE:host_ctf_insert>
            -D AVAILABILITY_PL_PATH=$<TARGET_FILE:availability.pl>
            -D MIG_PATH=${PUREDARWIN_SOURCE_DIR}/tools/mig/mig.sh
            -D MIGCOM_PATH=$<TARGET_FILE:migcom>
            -D STRIP_PATH=${XNU_STRIP_PATH}
            -D LIPO_PATH=${XNU_LIPO_PATH}
            -D NM_PATH=${XNU_NM_PATH}
            -D NMEDIT_PATH=$<TARGET_FILE:host_nmedit>
            -D LIBTOOL_PATH=$<TARGET_FILE:host_libtool>
            -D UNIFDEF_PATH=$<TARGET_FILE:unifdef>
            -D DSYMUTIL_PATH=${dsymutil_path}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/preprocess_files.cmake
    COMMAND
        # installhdrs builds XNU's SETUP helpers as native Linux host tools.
        # Do not export the Mach-O kernel linker here: native clang will pass
        # ELF linker options (for example --hash-style=gnu), which cctools
        # ld64 correctly rejects.
        ${CMAKE_COMMAND} -E env ${XNU_HOST_CC_ENV} --unset=LD --unset=CC --unset=CXX --unset=STRIP --unset=NM --unset=CTFCONVERT --unset=CTFMERGE --unset=CTFINSERT
        make -C <SOURCE_DIR> installhdrs ARCH_CONFIGS=${XNU_ARCH_CONFIGS} ${XNU_MACHINE_CONFIGS} BUILD_LTO=0
            BUILD_WERROR=0 DO_CTFMERGE=0 KERNEL_CONFIGS=${PUREDARWIN_XNU_KERNEL_CONFIG}
            SRCROOT=${CMAKE_CURRENT_SOURCE_DIR} OBJROOT=<BINARY_DIR>
            DSTROOT=${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install
    INSTALL_COMMAND "" # installed during build
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
)
add_dependencies(xnu_headers.extproj host_codesign_allocate
    migcom host_strip host_lipo host_nm unifdef host_nmedit
    host_ctfconvert host_ctfmerge availability.pl)

add_library(xnu_headers INTERFACE)
add_dependencies(xnu_headers xnu_headers.extproj)
target_include_directories(xnu_headers INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install/usr/include
)
add_library(xnu_private_headers INTERFACE)
add_dependencies(xnu_private_headers xnu_headers.extproj)
target_include_directories(xnu_private_headers INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install/usr/local/include
    ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders
)
target_compile_options(xnu_private_headers INTERFACE
    -F${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install/System/Library/Frameworks
)
add_library(xnu_kernel_headers INTERFACE)
add_dependencies(xnu_kernel_headers xnu_headers.extproj)
target_include_directories(xnu_kernel_headers INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install/System/Library/Frameworks/Kernel.framework/Versions/A/Headers
)
add_library(xnu_kernel_private_headers INTERFACE)
add_dependencies(xnu_kernel_private_headers xnu_headers.extproj)
target_include_directories(xnu_kernel_private_headers INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders
)

externalproject_add(xnu
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/xnu_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_COMMAND "" # disable auto CMake configure
    BUILD_COMMAND
        ${CMAKE_COMMAND}
            -D XNU_SRC=${CMAKE_CURRENT_SOURCE_DIR}
            -D XNU_OBJ=<BINARY_DIR>
            -D CODESIGN_ALLOCATE_PATH=$<TARGET_FILE:host_codesign_allocate>
            -D CTFCONVERT_PATH=$<TARGET_FILE:host_ctfconvert>
            -D CTFMERGE_PATH=$<TARGET_FILE:host_ctfmerge>
            -D CTFINSERT_PATH=$<TARGET_FILE:host_ctf_insert>
            -D AVAILABILITY_PL_PATH=$<TARGET_FILE:availability.pl>
            -D LD_PATH=${XNU_LD_PATH}
            -D MIG_PATH=${PUREDARWIN_SOURCE_DIR}/tools/mig/mig.sh
            -D MIGCOM_PATH=$<TARGET_FILE:migcom>
            -D STRIP_PATH=${XNU_STRIP_PATH}
            -D LIPO_PATH=${XNU_LIPO_PATH}
            -D NM_PATH=${XNU_NM_PATH}
            -D NMEDIT_PATH=$<TARGET_FILE:host_nmedit>
            -D LIBTOOL_PATH=$<TARGET_FILE:host_libtool>
            -D UNIFDEF_PATH=$<TARGET_FILE:unifdef>
            -D DSYMUTIL_PATH=${dsymutil_path}
            -D FIREHOSE_KERNEL_LIBRARY_PATH=$<TARGET_FILE:libfirehose_kernel>
            -D FIREHOSE_KERNEL_HEADER_PATH=$<TARGET_PROPERTY:libfirehose_kernel,INTERFACE_INCLUDE_DIRECTORIES>
            -D PTHREAD_HEADER_PATH=$<TARGET_PROPERTY:pthread_common_headers,INTERFACE_INCLUDE_DIRECTORIES>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/preprocess_files.cmake
    COMMAND
        ${CMAKE_COMMAND} -E env LD=${XNU_LD_PATH} STRIP=${XNU_STRIP_PATH} ${XNU_HOST_CC_ENV} --unset=CC --unset=CXX --unset=NM --unset=CTFCONVERT --unset=CTFMERGE --unset=CTFINSERT
        make ${XNU_MAKE_OUTPUT_SYNC_FLAG} -C <SOURCE_DIR> install ARCH_CONFIGS=${XNU_ARCH_CONFIGS} ${XNU_MACHINE_CONFIGS} BUILD_LTO=0
            BUILD_WERROR=0 DO_CTFMERGE=0 KERNEL_CONFIGS=${PUREDARWIN_XNU_KERNEL_CONFIG}
            SRCROOT=${CMAKE_CURRENT_SOURCE_DIR} OBJROOT=<BINARY_DIR>
            SYMROOT=${CMAKE_CURRENT_BINARY_DIR}/xnu_sym DSTROOT=${CMAKE_CURRENT_BINARY_DIR}/xnu
    INSTALL_COMMAND "" # installed during build
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
)
set(_xnu_dependencies
    xnu_headers libfirehose_kernel host_codesign_allocate host_ctfconvert host_ctfmerge
    migcom host_strip host_lipo host_nm unifdef host_nmedit availability.pl host_ld
    dyld_availability_headers
)
if(PUREDARWIN_NIX_TOOLCHAIN)
    list(REMOVE_ITEM _xnu_dependencies host_ld)
endif()
add_dependencies(xnu ${_xnu_dependencies})

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/System/Library/Frameworks/Kernel.framework/Versions/A/Resources DESTINATION System/Library/Frameworks/Kernel.framework/Versions/A COMPONENT BaseSystem)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/System/Library/Frameworks/Kernel.framework/Versions/A/Headers DESTINATION System/Library/Frameworks/Kernel.framework/Versions/A COMPONENT DeveloperTools)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders DESTINATION System/Library/Frameworks/Kernel.framework/Versions/A COMPONENT DeveloperInternal)

# Note: This really ought to be associated with a custom build command like 'install_framework_symlinks' or something
install_symlink(System/Library/Frameworks/Kernel.framework/Versions/Current Versions/A COMPONENT BaseSystem)
install_symlink(System/Library/Frameworks/Kernel.framework/Resources Versions/Current/Resources COMPONENT BaseSystem)
install_symlink(System/Library/Frameworks/Kernel.framework/Headers Versions/Current/Headers COMPONENT DeveloperTools)
install_symlink(System/Library/Frameworks/Kernel.framework/PrivateHeaders Versions/Current COMPONENT DeveloperInternal)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders DESTINATION System/Library/Frameworks/System.framework/Versions/B COMPONENT DeveloperInternal)

install_symlink(System/Library/Frameworks/System.framework/Versions/Current Versions/B COMPONENT BaseSystem)
install_symlink(System/Library/Frameworks/System.framework/PrivateHeaders Versions/Current COMPONENT DeveloperInternal)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/System/Library/Frameworks/DriverKit.framework/Versions/A/Headers DESTINATION System/Library/Frameworks/DriverKit.framework/Versions/A COMPONENT DeveloperTools)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/System/Library/Frameworks/DriverKit.framework/Versions/A/PrivateHeaders DESTINATION System/Library/Frameworks/DriverKit.framework/Versions/A COMPONENT DeveloperInternal)

install_symlink(System/Library/Frameworks/DriverKit.framework/Versions/Current Versions/A COMPONENT BaseSystem)
install_symlink(System/Library/Frameworks/DriverKit.framework/Headers Versions/Current/Headers COMPONENT DeveloperTools)
install_symlink(System/Library/Frameworks/DriverKit.framework/PrivateHeaders Versions/Current COMPONENT DeveloperInternal)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/System/Library/Frameworks/IOKit.framework/Versions/A/Headers DESTINATION System/Library/Frameworks/IOKit.framework/Versions/A COMPONENT DeveloperTools)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/System/Library/Frameworks/IOKit.framework/Versions/A/PrivateHeaders DESTINATION System/Library/Frameworks/IOKit.framework/Versions/A COMPONENT DeveloperInternal)

install_symlink(System/Library/Frameworks/IOKit.framework/Versions/Current Versions/A COMPONENT BaseSystem)
install_symlink(System/Library/Frameworks/IOKit.framework/Headers Versions/Current/Headers COMPONENT DeveloperTools)
install_symlink(System/Library/Frameworks/IOKit.framework/PrivateHeaders Versions/Current COMPONENT DeveloperInternal)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/System/Library/Kernels DESTINATION System/Library COMPONENT BaseSystem)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/System/Library/Extensions DESTINATION System/Library COMPONENT BaseSystem)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/usr/include DESTINATION usr COMPONENT DeveloperTools)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/usr/lib DESTINATION usr COMPONENT BaseSystem)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/usr/libexec DESTINATION usr COMPONENT BaseSystem)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/usr/share/man DESTINATION usr/share COMPONENT BaseSystem)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/usr/share/misc DESTINATION usr/share COMPONENT BaseSystem)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/usr/local DESTINATION usr COMPONENT DeveloperInternal)
