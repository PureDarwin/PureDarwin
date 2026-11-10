# empty kexts under the bundle ids Inferno's kernel patcher reads __TEXT_EXEC from
# nothing matches or loads them
set(PD_INFERNO_STUBS
    AppleMobileFileIntegrity=com.apple.driver.AppleMobileFileIntegrity
    AppleImage4=com.apple.security.AppleImage4
    Image4=com.apple.security.Image4
    AppleFirmwareUpdateKext=com.apple.driver.AppleFirmwareUpdateKext
    AppleS5L8960XWatchDogTimer=com.apple.driver.AppleS5L8960XWatchDogTimer
)
foreach(stub IN LISTS PD_INFERNO_STUBS)
    string(REPLACE "=" ";" parts "${stub}")
    list(GET parts 0 stub_name)
    list(GET parts 1 stub_id)
    set(target PDStub${stub_name}.kext)
    add_kext_bundle(${target} INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
        BUNDLE_IDENTIFIER ${stub_id} BUNDLE_VERSION 1.0.0
        MAIN_FUNCTION pd_inferno_stub_start ANTIMAIN_FUNCTION pd_inferno_stub_stop)
    set_property(TARGET ${target} PROPERTY OUTPUT_NAME PDStub${stub_name})
    target_sources(${target} PRIVATE stub.c)
    install(TARGETS ${target} DESTINATION System/Library/Extensions COMPONENT BaseSystem)
    add_dependencies(kexts ${target})
endforeach()
