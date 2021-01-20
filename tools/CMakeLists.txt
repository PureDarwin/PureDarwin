set(PUREDARWIN_TOOLS_DIR ${CMAKE_CURRENT_BINARY_DIR}/prefix)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/prefix)

add_subdirectory(cctools)
add_subdirectory(xar)
