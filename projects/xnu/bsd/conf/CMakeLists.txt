add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Makefile
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/config
    COMMAND xnu_doconf -c -cpu x86_64 -platform x86_64 -platform MacOSX -soc "" -d ${CMAKE_CURRENT_BINARY_DIR}/config -s ${CMAKE_CURRENT_SOURCE_DIR} -m ${PUREDARWIN_SOURCE_DIR}/projects/xnu/config RELEASE
    DEPENDS xnu_doconf COMMENT "Run xnu_doconf for bsd component" VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/config/config-prefix.h
    COMMAND ${CMAKE_COMMAND} -E env INPUT=${CMAKE_CURRENT_BINARY_DIR}/Makefile OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/config/config-prefix.h ${PUREDARWIN_SOURCE_DIR}/projects/xnu/SETUP/makefile_to_prefix.sh
    DEPENDS ${PUREDARWIN_SOURCE_DIR}/projects/xnu/SETUP/makefile_to_prefix.sh ${CMAKE_CURRENT_BINARY_DIR}/Makefile
    COMMENT "Generate bsd config-prefix.h" VERBATIM
)

add_library(xnu.bsd_conf INTERFACE)
target_include_directories(xnu.bsd_conf INTERFACE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(xnu.bsd_conf INTERFACE -include ${CMAKE_CURRENT_BINARY_DIR}/config/config-prefix.h)
add_dependencies(xnu.bsd_conf ${CMAKE_CURRENT_BINARY_DIR}/Makefile ${CMAKE_CURRENT_BINARY_DIR}/config/config-prefix.h)
