find_program(dsymutil_path NAMES dsymutil llvm-dsymutil REQUIRED)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install)
externalproject_add(xnu_headers.extproj
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_COMMAND true # disable auto CMake configure
    BUILD_COMMAND
        cmake
            -D XNU_SRC=${CMAKE_CURRENT_SOURCE_DIR}
            -D XNU_OBJ=<BINARY_DIR>
            -D CODESIGN_ALLOCATE_PATH=$<TARGET_FILE:darwin_codesign_allocate>
            -D CTFCONVERT_PATH=$<TARGET_FILE:ctfconvert.host>
            -D CTFMERGE_PATH=$<TARGET_FILE:ctfmerge.host>
            -D CTFINSERT_PATH=$<TARGET_FILE:host_ctf_insert>
            -D AVAILABILITY_PL_PATH=$<TARGET_FILE:availability.pl>
            -D MIG_PATH=${PUREDARWIN_SOURCE_DIR}/tools/mig/mig.sh
            -D MIGCOM_PATH=$<TARGET_FILE:migcom>
            -D STRIP_PATH=$<TARGET_FILE:host_strip>
            -D LIPO_PATH=$<TARGET_FILE:host_lipo>
            -D NM_PATH=$<TARGET_FILE:host_nm>
            -D NMEDIT_PATH=$<TARGET_FILE:host_nmedit>
            -D LIBTOOL_PATH=$<TARGET_FILE:host_libtool>
            -D UNIFDEF_PATH=$<TARGET_FILE:unifdef>
            -D DSYMUTIL_PATH=${dsymutil_path}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/preprocess_files.cmake
    COMMAND
        make -C <SOURCE_DIR> installhdrs ARCH_CONFIGS=X86_64 BUILD_LTO=0
            BUILD_WERROR=0 DO_CTFMERGE=0 KERNEL_CONFIGS=RELEASE
            SRCROOT=${CMAKE_CURRENT_SOURCE_DIR} OBJROOT=<BINARY_DIR>
            DSTROOT=${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install
    INSTALL_COMMAND true # installed during build
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
)
add_dependencies(xnu_headers.extproj darwin_codesign_allocate
    migcom host_strip host_lipo host_nm unifdef host_nmedit
    ctfconvert.host ctfmerge.host availability.pl)

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
    CONFIGURE_COMMAND true # disable auto CMake configure
    BUILD_COMMAND
        cmake
            -D XNU_SRC=${CMAKE_CURRENT_SOURCE_DIR}
            -D XNU_OBJ=<BINARY_DIR>
            -D CODESIGN_ALLOCATE_PATH=$<TARGET_FILE:darwin_codesign_allocate>
            -D CTFCONVERT_PATH=$<TARGET_FILE:ctfconvert.host>
            -D CTFMERGE_PATH=$<TARGET_FILE:ctfmerge.host>
            -D CTFINSERT_PATH=$<TARGET_FILE:host_ctf_insert>
            -D AVAILABILITY_PL_PATH=$<TARGET_FILE:availability.pl>
            -D LD_PATH=$<TARGET_FILE:darwin_ld>
            -D MIG_PATH=${PUREDARWIN_SOURCE_DIR}/tools/mig/mig.sh
            -D MIGCOM_PATH=$<TARGET_FILE:migcom>
            -D STRIP_PATH=$<TARGET_FILE:host_strip>
            -D LIPO_PATH=$<TARGET_FILE:host_lipo>
            -D NM_PATH=$<TARGET_FILE:host_nm>
            -D NMEDIT_PATH=$<TARGET_FILE:host_nmedit>
            -D LIBTOOL_PATH=$<TARGET_FILE:host_libtool>
            -D UNIFDEF_PATH=$<TARGET_FILE:unifdef>
            -D DSYMUTIL_PATH=${dsymutil_path}
            -D FIREHOSE_KERNEL_LIBRARY_PATH=$<TARGET_FILE:libfirehose_kernel>
            -D FIREHOSE_KERNEL_HEADER_PATH=$<TARGET_PROPERTY:libfirehose_kernel,INTERFACE_INCLUDE_DIRECTORIES>
            -D PTHREAD_HEADER_PATH=$<TARGET_PROPERTY:pthread_common_headers,INTERFACE_INCLUDE_DIRECTORIES>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/preprocess_files.cmake
    COMMAND
        make -C <SOURCE_DIR> install ARCH_CONFIGS=X86_64 BUILD_LTO=0
            BUILD_WERROR=0 DO_CTFMERGE=0 KERNEL_CONFIGS=RELEASE
            SRCROOT=${CMAKE_CURRENT_SOURCE_DIR} OBJROOT=<BINARY_DIR>
            SYMROOT=${CMAKE_CURRENT_BINARY_DIR}/xnu_sym DSTROOT=${CMAKE_CURRENT_BINARY_DIR}/xnu
    INSTALL_COMMAND true # installed during build
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
)
add_dependencies(xnu xnu_headers libfirehose_kernel darwin_codesign_allocate ctfconvert.host ctfmerge.host migcom host_strip host_lipo host_nm unifdef host_nmedit availability.pl darwin_ld)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/System/Library/Kernels DESTINATION System/Library COMPONENT BaseSystem)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/System/Library/Extensions DESTINATION System/Library COMPONENT BaseSystem)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/System/Library/Frameworks/Kernel.framework/Versions/A/Resources DESTINATION System/Library/Frameworks/Kernel.framework/Versions/A COMPONENT BaseSystem)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/System/Library/Frameworks/Kernel.framework/Versions/A/Headers DESTINATION System/Library/Frameworks/Kernel.framework/Versions/A COMPONENT DeveloperTools)
install_symlink(/System/Library/Frameworks/Kernel.framework/Versions/Current A COMPONENT BaseSystem)
install_symlink(/System/Library/Frameworks/Kernel.framework/Resources Versions/Current/Resources COMPONENT BaseSystem)
install_symlink(/System/Library/Frameworks/Kernel.framework/Headers Versions/Current/Headers COMPONENT DeveloperTools)


install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/usr/include DESTINATION usr COMPONENT DeveloperTools)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/usr/lib/dtrace DESTINATION usr/lib COMPONENT DeveloperTools)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/usr/libexec DESTINATION usr COMPONENT DeveloperTools)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/usr/share/man DESTINATION usr/share COMPONENT BaseSystem)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu/usr/share/misc DESTINATION usr/share COMPONENT BaseSystem)


add_subdirectory(libkern/kmod)
