add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/base.stamp
    COMMAND ${CMAKE_COMMAND} -D OUTDIR=${CMAKE_CURRENT_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/download-base.cmake
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/base.stamp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/download-base.cmake
    COMMENT "Download FreeBSD base archive" VERBATIM USES_TERMINAL
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kernel.stamp
    COMMAND ${CMAKE_COMMAND} -D OUTDIR=${CMAKE_CURRENT_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/download-kernel.cmake
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/kernel.stamp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/download-kernel.cmake
    COMMENT "Download FreeBSD kernel archive" VERBATIM USES_TERMINAL
)

add_custom_target(fbsd_base DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/base.stamp)
add_custom_target(fbsd_kernel DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kernel.stamp)
