add_kext_bundle(Sandbox.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.security.sandbox BUNDLE_VERSION 1.0.0
    MAIN_FUNCTION sandbox_kext_start ANTIMAIN_FUNCTION sandbox_kext_stop)
set_property(TARGET Sandbox.kext PROPERTY OUTPUT_NAME Sandbox)

target_sources(Sandbox.kext PRIVATE
    sandbox_kext.c
)

# The compiled profile format is shared with libsandbox.
target_include_directories(Sandbox.kext PRIVATE
    ${PUREDARWIN_SOURCE_DIR}/src/Libraries/libsandbox
)

install(TARGETS Sandbox.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
