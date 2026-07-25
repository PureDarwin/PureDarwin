if(NOT PUREDARWIN_ENABLE_OSMESA_FB)
    return()
endif()

if(NOT PUREDARWIN_OSMESA_PREFIX AND DEFINED ENV{PUREDARWIN_OSMESA_PREFIX})
    set(PUREDARWIN_OSMESA_PREFIX "$ENV{PUREDARWIN_OSMESA_PREFIX}")
endif()
if(NOT PUREDARWIN_OSMESA_PREFIX)
    message(FATAL_ERROR "PUREDARWIN_OSMESA_PREFIX must point to the Mesa install 'usr' prefix (with include/GL and lib/libOSMesa.8.dylib)")
endif()

add_darwin_executable(osmesa-fb MACOSX_VERSION_MIN 11.0)
target_sources(osmesa-fb PRIVATE osmesa_fb.c pdgl_surface.c)

# These TUs pull the SDK's GL + PDGOP headers, not PD's -nostdinc header set.
get_target_property(_osmesa_fb_opts osmesa-fb COMPILE_OPTIONS)
if(_osmesa_fb_opts)
    list(REMOVE_ITEM _osmesa_fb_opts -nostdinc)
    set_target_properties(osmesa-fb PROPERTIES COMPILE_OPTIONS "${_osmesa_fb_opts}")
endif()

target_include_directories(osmesa-fb PRIVATE
    "${CMAKE_SOURCE_DIR}/src/Libraries/PDGOP/include"
    "${PUREDARWIN_OSMESA_PREFIX}/include")
target_compile_options(osmesa-fb PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK} -Wno-everything)

target_link_options(osmesa-fb PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,11.0,11.5
    -Wl,-fixup_chains
    # gl* entrypoints live in libglapi (shared-glapi); OSMesa* in libOSMesa.
    # Linked by path; their baked-in install_names (/usr/lib/...) become the
    # runtime load deps, satisfied from the image.
    "${PUREDARWIN_OSMESA_PREFIX}/lib/libOSMesa.8.dylib"
    "${PUREDARWIN_OSMESA_PREFIX}/lib/libglapi.0.dylib"
    -lSystem)
target_link_libraries(osmesa-fb PRIVATE $<TARGET_FILE:PDGOP> $<TARGET_FILE:IOKit>)
add_dependencies(osmesa-fb PDGOP IOKit libSystem_B_stub dyld)

install(TARGETS osmesa-fb DESTINATION usr/bin COMPONENT BaseSystem)
