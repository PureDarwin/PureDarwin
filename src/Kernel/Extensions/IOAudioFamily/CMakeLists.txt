## IOAudioFamily: Apple's audio driver family (APSL 1.1), vendored unmodified
## from apple-oss-distributions/IOAudioFamily.
##
## This is what turns a single-stream HDA driver into a real audio device: the
## family does per-client buffer registration, mixing (IOAudioEngineMixer.cpp),
## format conversion and the sample-clock timestamps that userland needs to
## schedule against. Hardware drivers subclass IOAudioDevice/IOAudioEngine.
##
## Same vendoring pattern as IOGraphicsFamily/IOPCIFamily. Old-style IOKit C++
## with no STL, so it does not hit the C++-in-kext problems the gen9 graphics
## work ran into.

## No MAIN_FUNCTION: IOAudioFamily is a pure IOKit family and defines no module
## start routine, unlike IOGraphicsFamily. Naming one that does not exist still
## links (kexts resolve undefined symbols at load) and then panics in the start
## stub, which is what happened the first time this was wired up. Every other
## family here - IOPCIFamily, IOStorageFamily, IOHIDFamily, IONetworkingFamily -
## likewise declares none.
add_kext_bundle(IOAudioFamily.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.iokit.IOAudioFamily BUNDLE_VERSION 300.6.1)
set_property(TARGET IOAudioFamily.kext PROPERTY OUTPUT_NAME IOAudioFamily)

target_sources(IOAudioFamily.kext PRIVATE
    BigNum128.cpp
    IOAudioControl.cpp
    IOAudioControlUserClient.cpp
    IOAudioDevice.cpp
    IOAudioEngine.cpp
    IOAudioEngineMixer.cpp
    IOAudioEngineUserClient.cpp
    IOAudioLevelControl.cpp
    IOAudioPort.cpp
    IOAudioSelectorControl.cpp
    IOAudioStream.cpp
    IOAudioTimeIntervalFilter.cpp
    IOAudioToggleControl.cpp)

target_include_directories(IOAudioFamily.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    # Drivers include these as <IOKit/audio/IOAudioEngine.h>; the IOKit/audio
    # directory is a symlink view onto the headers beside the sources.
    ${CMAKE_CURRENT_SOURCE_DIR}/IOKit
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_link_libraries(IOAudioFamily.kext PRIVATE xnu_kernel_private_headers)

# Force-included so the vendored Apple sources stay unmodified; see the header
# for why the published sources need it against this XNU.
target_compile_options(IOAudioFamily.kext PRIVATE
    -include${CMAKE_CURRENT_SOURCE_DIR}/pd-iomalloc-compat.h)

target_compile_options(IOAudioFamily.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual
    -Wno-deprecated-declarations -Wno-unused-parameter
    -Wno-inconsistent-missing-override -Wno-overloaded-virtual)
