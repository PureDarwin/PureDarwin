add_kext_bundle(ApfsFileSystemDriver.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.ravynos.driver.ApfsFileSystemDriver BUNDLE_VERSION 1.0.0)
set_property(TARGET ApfsFileSystemDriver.kext PROPERTY OUTPUT_NAME ApfsFileSystemDriver)

# libapfsrw's kernel binding, as apfs.kext builds it: selector 0 adds volumes through it
set(LIBAPFSRW_DIR ${CMAKE_SOURCE_DIR}/projects/libapfsrw)

target_sources(ApfsFileSystemDriver.kext PRIVATE
    ApfsFileSystemDriver.cpp
    ${LIBAPFSRW_DIR}/src/apfsrw.c
    ${LIBAPFSRW_DIR}/src/apfsrw_kern.c
    ${LIBAPFSRW_DIR}/src/apfsrw_name.c
)

set_source_files_properties(
    ${LIBAPFSRW_DIR}/src/apfsrw.c
    ${LIBAPFSRW_DIR}/src/apfsrw_kern.c
    ${LIBAPFSRW_DIR}/src/apfsrw_name.c
    PROPERTIES COMPILE_DEFINITIONS APFSRW_KERNEL=1)

target_include_directories(ApfsFileSystemDriver.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include
    ${LIBAPFSRW_DIR}/include
    ${LIBAPFSRW_DIR}/third_party/utf8proc)
target_link_libraries(ApfsFileSystemDriver.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(ApfsFileSystemDriver.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual)
set_property(TARGET ApfsFileSystemDriver.kext PROPERTY CXX_STANDARD 14)
install(TARGETS ApfsFileSystemDriver.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
