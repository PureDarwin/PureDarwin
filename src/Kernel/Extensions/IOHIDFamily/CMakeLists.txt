# IOHIDFamily: Apple's HID stack (IOHIDFamily-1633.140.4), as ported by
# ravynOS to build against a Catalina-era xnu.
add_kext_bundle(IOHIDFamily.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.iokit.IOHIDFamily BUNDLE_VERSION 1.5.2)
set_property(TARGET IOHIDFamily.kext PROPERTY OUTPUT_NAME IOHIDFamily)

target_sources(IOHIDFamily.kext PRIVATE
    IOHIDSystem/IOHIDevice.cpp
    IOHIDSystem/IOHIKeyboard.cpp
    IOHIDSystem/IOHIKeyboardMapper.cpp
    IOHIDSystem/IOBSDConsole.cpp
    IOHIDSystem/IOFixed64.cpp
    IOHIDSystem/IOFixedPoint64.cpp
    IOHIDSystem/IOHIDPrivate.cpp
    IOHIDSystem/IOHIDSystem.cpp
    IOHIDSystem/IOHIDSystemCursorHelper.cpp
    IOHIDSystem/IOHIDUserClient.cpp
    IOHIDSystem/IOHIDWorkLoop.cpp
    IOHIDSystem/PDHIDEventQueue.cpp
)

target_include_directories(IOHIDFamily.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    # include/ shims: IOKit/hid -> IOHIDFamily headers, IOKit/hidevent,
    # IOKit/graphics/IOGraphicsTypes.h (from ravynOS IOGraphics), assert.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/IOHIDFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/IOHIDSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/IOHIDSystem/IOKit/hidsystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS
    ${CMAKE_CURRENT_BINARY_DIR}/gen)
target_link_libraries(IOHIDFamily.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(IOHIDFamily.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual
    -Wno-deprecated-declarations)
set_property(TARGET IOHIDFamily.kext PROPERTY CXX_STANDARD 14)
install(TARGETS IOHIDFamily.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
