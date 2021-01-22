find_package(OpenSSL REQUIRED)

externalproject_add(xar_host.extproj
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}
    GIT_REPOSITORY https://github.com/tpoechtrager/xar
    GIT_TAG origin/master

    CONFIGURE_COMMAND
        env CFLAGS=-w CC=clang CPPFLAGS=-I${OPENSSL_ROOT_DIR}/include LDFLAGS=-L${OPENSSL_ROOT_DIR}/lib
            <SOURCE_DIR>/xar/configure --prefix=${PUREDARWIN_TOOLS_DIR}
    BUILD_COMMAND make
    INSTALL_COMMAND true
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
    USES_TERMINAL_INSTALL TRUE
)

add_executable(xar_host IMPORTED)
add_dependencies(xar_host xar_host.extproj)
set_property(TARGET xar_host PROPERTY IMPORTED_LOCATION ${PUREDARWIN_TOOLS_DIR}/bin/xar)

add_library(libxar_host IMPORTED STATIC)
add_dependencies(libxar_host xar_host.extproj)
set_property(TARGET libxar_host PROPERTY IMPORTED_LOCATION ${PUREDARWIN_TOOLS_DIR}/lib/libxar.a)
target_include_directories(libxar_host INTERFACE ${PUREDARWIN_TOOLS_DIR}/include)
