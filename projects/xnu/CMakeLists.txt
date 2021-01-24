set(XNU_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_install)
externalproject_add(xnu_headers
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/xnu_header_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_COMMAND
        cmake
            -D XNU_SRC=${CMAKE_CURRENT_SOURCE_DIR}
            -D CODESIGN_ALLOCATE_PATH=$<TARGET_FILE:codesign_allocate_host>
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
