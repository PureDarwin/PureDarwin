set(PUREDARWIN_TOOLS_DIR ${CMAKE_CURRENT_BINARY_DIR}/prefix)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/prefix)

add_subdirectory(cctools)
add_subdirectory(dtrace_ctf)
add_subdirectory(mig)
add_subdirectory(unifdef)
add_subdirectory(xar)
