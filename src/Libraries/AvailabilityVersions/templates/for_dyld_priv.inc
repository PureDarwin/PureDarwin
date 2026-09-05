

// @@DYLD_HEADER_VERSIONS()@@

// These exist to maintain source compat as we cleanup headers
// TODO: These should be marked for macro deprecation and eventually removed
#define DYLD_MACOSX_VERSION_11_00               DYLD_MACOSX_VERSION_11_0
#define DYLD_MACOSX_VERSION_12_00               DYLD_MACOSX_VERSION_12_0
#define dyld_platform_version_macOS_11_00       dyld_platform_version_macOS_11_0
#define dyld_platform_version_macOS_12_00       dyld_platform_version_macOS_12_0




#if 0
#pragma clang deprecated(DYLD_MACOSX_VERSION_11_00, "use DYLD_MACOSX_VERSION_11_0 instead")
#pragma clang deprecated(DYLD_MACOSX_VERSION_12_00, "use DYLD_MACOSX_VERSION_12_0 instead")
#pragma clang deprecated(dyld_platform_version_macOS_11_00, "use dyld_platform_version_macOS_11_0 instead")
#pragma clang deprecated(dyld_platform_version_macOS_12_00, "use dyld_platform_version_macOS_12_0 instead")
#endif
