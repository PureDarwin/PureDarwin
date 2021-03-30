function(xnu_run_doconf name)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Makefile
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/config
        COMMAND xnu_doconf -c -cpu x86_64 -platform x86_64 -platform MacOSX -soc "" -d ${CMAKE_CURRENT_BINARY_DIR}/config -s ${CMAKE_CURRENT_SOURCE_DIR} -m ${PUREDARWIN_SOURCE_DIR}/projects/xnu/config RELEASE
        DEPENDS xnu_doconf COMMENT "xnu_doconf ${name}" VERBATIM
    )

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/config/config-prefix.h
        COMMAND ${CMAKE_COMMAND} -E env INPUT=${CMAKE_CURRENT_BINARY_DIR}/Makefile OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/config/config-prefix.h ${PUREDARWIN_SOURCE_DIR}/projects/xnu/SETUP/makefile_to_prefix.sh
        DEPENDS ${PUREDARWIN_SOURCE_DIR}/projects/xnu/SETUP/makefile_to_prefix.sh ${CMAKE_CURRENT_BINARY_DIR}/Makefile
        COMMENT "Generate ${name} config-prefix.h" VERBATIM
    )

    add_custom_target(xnu.doconf_${name} DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Makefile ${CMAKE_CURRENT_BINARY_DIR}/config/config-prefix.h)

    add_library(xnu.${name}_conf INTERFACE)
    target_include_directories(xnu.${name}_conf INTERFACE ${CMAKE_CURRENT_BINARY_DIR})
    target_compile_options(xnu.${name}_conf INTERFACE -include ${CMAKE_CURRENT_BINARY_DIR}/config/config-prefix.h)
    add_dependencies(xnu.${name}_conf xnu.doconf_${name})
endfunction()

add_subdirectory(SETUP)
add_subdirectory(bsd)








#[[
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install)
externalproject_add(xnu_headers.extproj
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_COMMAND
        cmake
            -D XNU_SRC=${CMAKE_CURRENT_SOURCE_DIR}
            -D CODESIGN_ALLOCATE_PATH=$<TARGET_FILE:darwin_codesign_allocate>
            -D CTFCONVERT_PATH=$<TARGET_FILE:ctfconvert.host>
            -D CTFMERGE_PATH=$<TARGET_FILE:ctfmerge.host>
            -D AVAILABILITY_PL_PATH=$<TARGET_FILE:availability.pl>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/preprocess_files.cmake
    BUILD_COMMAND
        make -C <SOURCE_DIR> installhdrs ARCH_CONFIGS=X86_64 BUILD_LTO=0
            BUILD_WERROR=0 DO_CTFMERGE=0 KERNEL_CONFIGS=RELEASE
            SRCROOT=${CMAKE_CURRENT_SOURCE_DIR} OBJROOT=<BINARY_DIR>
            DSTROOT=${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install
    INSTALL_COMMAND true # installed during build
    UPDATE_COMMAND "" # prevent constant rebuilds
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
)

add_library(xnu_headers INTERFACE)
add_dependencies(xnu_headers xnu_headers.extproj)
target_include_directories(xnu_headers INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install/usr/include
)
add_library(xnu_private_headers INTERFACE)
add_dependencies(xnu_private_headers xnu_headers.extproj)
target_include_directories(xnu_private_headers INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install/usr/local/include
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
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/xnu
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_COMMAND
        cmake
            -D XNU_SRC=${CMAKE_CURRENT_SOURCE_DIR}
            -D CODESIGN_ALLOCATE_PATH=$<TARGET_FILE:darwin_codesign_allocate>
            -D CTFCONVERT_PATH=$<TARGET_FILE:ctfconvert.host>
            -D CTFMERGE_PATH=$<TARGET_FILE:ctfmerge.host>
            -D AVAILABILITY_PL_PATH=$<TARGET_FILE:availability.pl>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/preprocess_files.cmake
    BUILD_COMMAND
        make -C <SOURCE_DIR> install ARCH_CONFIGS=X86_64 BUILD_LTO=0
            BUILD_WERROR=0 DO_CTFMERGE=0 KERNEL_CONFIGS=RELEASE
            SRCROOT=${CMAKE_CURRENT_SOURCE_DIR} OBJROOT=<BINARY_DIR>
            DSTROOT=${CMAKE_CURRENT_BINARY_DIR}/xnu
    INSTALL_COMMAND true # installed during build
    UPDATE_COMMAND "" # prevent constant rebuilds
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
)
add_dependencies(xnu xnu_headers libfirehose_kernel)
]]#
