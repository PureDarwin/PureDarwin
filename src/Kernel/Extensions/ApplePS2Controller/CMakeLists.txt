add_kext_bundle(ApplePS2Controller INFO_PLIST Info.plist BUNDLE_IDENTIFIER com.apple.driver.ApplePS2Controller BUNDLE_VERSION 1.1.0d1)
target_sources(ApplePS2Controller PRIVATE
    ApplePS2Controller.cpp
    ApplePS2KeyboardDevice.cpp
    ApplePS2MouseDevice.cpp
    ApplePS2Keyboard.cpp
    ApplePS2Mouse.cpp
)

# ApplePS2Keyboard subclasses IOHIKeyboard (IOKit/hidsystem headers + class
# from the IOHIDFamily kext).
target_include_directories(ApplePS2Controller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/include)

set_property(TARGET ApplePS2Controller PROPERTY CXX_STANDARD 11)
target_compile_options(ApplePS2Controller PRIVATE -Wno-implicit-int-conversion)
install(TARGETS ApplePS2Controller DESTINATION System/Library/Extensions COMPONENT BaseSystem)
