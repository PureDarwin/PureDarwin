# needs IOKit.framework, which only its own build and fbdoom enable
if(NOT TARGET IOKit)
    return()
endif()

add_darwin_static_library(PDGOP)

target_sources(PDGOP PRIVATE
    PDGOP.c
)

target_include_directories(PDGOP
    PUBLIC include
    PRIVATE ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libsystem_kernel/mach
        ${PUREDARWIN_COREFOUNDATION_PREFIX}/include
)

target_link_libraries(PDGOP PUBLIC IOKit)

install(TARGETS PDGOP DESTINATION usr/local/lib COMPONENT DeveloperInternal)
install(FILES include/PDGOP.h DESTINATION usr/local/include COMPONENT DeveloperInternal)
