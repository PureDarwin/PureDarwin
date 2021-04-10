add_subdirectory(atm)
add_subdirectory(gssd)
add_subdirectory(libsa)

add_darwin_object_library(xnu.osfmk_module)
target_sources(xnu.osfmk_module PRIVATE
    $<TARGET_OBJECTS:xnu.osfmk_atm_module>
)

target_include_directories(xnu.osfmk_module PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
