add_kext_bundle(PDACPIPlatform KERNEL_PRIVATE INFO_PLIST Info.plist
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDACPIPlatform BUNDLE_VERSION 1.0.0)

file(GLOB PDACPI_UACPI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/uacpi/source/*.c)

target_sources(PDACPIPlatform PRIVATE
    PDACPIPlatformExpert.cpp
    PDACPIGlue.cpp
    AppleI386CPU.cpp
    ${PDACPI_UACPI_SOURCES}
)

target_include_directories(PDACPIPlatform PRIVATE uacpi/include)

# uACPI is freestanding C: no libc, and the kernel has no <stdint.h> of the
# hosted kind. UACPI_BAREBONES_MODE keeps it to table access only for now.
target_compile_definitions(PDACPIPlatform PRIVATE UACPI_BAREBONES_MODE)
set_source_files_properties(${PDACPI_UACPI_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-std=c11;-ffreestanding")

target_link_libraries(PDACPIPlatform PRIVATE IOACPIFamilyHeaders)
target_compile_options(PDACPIPlatform PRIVATE -Wno-implicit-int-conversion -Wno-sign-conversion)
install(TARGETS PDACPIPlatform DESTINATION System/Library/Extensions COMPONENT BaseSystem)
