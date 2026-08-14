if(NOT PUREDARWIN_ENABLE_SYMPTOMREPORTER)
    return()
endif()

add_darwin_shared_library(SymptomReporter
    MACOSX_VERSION_MIN 11.0
    INSTALL_NAME /System/Library/PrivateFrameworks/SymptomReporter.framework/Versions/A/SymptomReporter)

target_sources(SymptomReporter PRIVATE
    symptom_reporter.c
)

target_include_directories(SymptomReporter PUBLIC include)

target_include_directories(SymptomReporter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_trace/os-include
    # os/log.h reaches os/object.h, which lives in xnu's libkern rather than
    # alongside it, and the generated libc os/object.h in turn expects
    # libdispatch's copy.
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/libkern
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/pd-compat-include
)

target_link_options(SymptomReporter PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-platform_version,macos,11.0,11.5
    -lSystem)

# The header-carrying interface targets libc's own headers need; without them
# <stdint.h> cannot reach the xnu sys/_types pieces it includes.
target_link_libraries(SymptomReporter PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
)

add_dependencies(SymptomReporter libc_features libSystem_B_stub)

install(TARGETS SymptomReporter
    DESTINATION System/Library/PrivateFrameworks/SymptomReporter.framework/Versions/A
    COMPONENT BaseSystem)
