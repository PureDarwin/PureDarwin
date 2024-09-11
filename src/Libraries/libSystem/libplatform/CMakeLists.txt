add_library(libplatform_headers INTERFACE)
target_include_directories(libplatform_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(libplatform_private_headers INTERFACE)
target_include_directories(libplatform_private_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/private
    ${CMAKE_CURRENT_SOURCE_DIR}/internal)


install(
    FILES include/setjmp.h include/string_x86.h include/ucontext.h
    DESTINATION usr/include COMPONENT DeveloperTools
)


add_subdirectory(dyld)
add_subdirectory(man)
add_subdirectory(src)
