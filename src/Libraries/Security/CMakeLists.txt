if(NOT PUREDARWIN_ENABLE_SECURITY)
    return()
endif()

foreach(_sec_prefix COREFOUNDATION LIBOBJC FOUNDATION SQLITE)
    if(NOT PUREDARWIN_${_sec_prefix}_PREFIX AND DEFINED ENV{PUREDARWIN_${_sec_prefix}_PREFIX})
        set(PUREDARWIN_${_sec_prefix}_PREFIX "$ENV{PUREDARWIN_${_sec_prefix}_PREFIX}")
    endif()
    if(NOT PUREDARWIN_${_sec_prefix}_PREFIX)
        message(FATAL_ERROR "PUREDARWIN_${_sec_prefix}_PREFIX must point to an install prefix")
    endif()
endforeach()

set(_sec apple)

set(_sec_farm ${CMAKE_CURRENT_BINARY_DIR}/farm)
file(GLOB_RECURSE _sec_headers ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/*.h)
file(MAKE_DIRECTORY ${_sec_farm}/Security)
foreach(_h ${_sec_headers})
    get_filename_component(_hn ${_h} NAME)
    file(CREATE_LINK ${_h} ${_sec_farm}/Security/${_hn} SYMBOLIC)
endforeach()

# Apple's base/Security.h would land in the farm as Security.h and drag in the
# CDSA headers; our umbrella takes its place for our own sources as well as for
# consumers. Authorization.h is ours too - libsecurity_authorization is not
# vendored. Everything else, including the keychain headers, is Apple's own.
file(REMOVE ${_sec_farm}/Security/Security.h)
foreach(_h Security.h Authorization.h)
    file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/include/Security/${_h}
         ${_sec_farm}/Security/${_h} SYMBOLIC)
endforeach()

function(_sec_farm_dir subdir)
    file(MAKE_DIRECTORY ${_sec_farm}/${subdir})
    foreach(_h ${ARGN})
        get_filename_component(_hn ${_h} NAME)
        file(CREATE_LINK ${_h} ${_sec_farm}/${subdir}/${_hn} SYMBOLIC)
    endforeach()
endfunction()

file(GLOB _sec_utils ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/OSX/utilities/*.h)
_sec_farm_dir(security_utilities ${_sec_utils})
file(GLOB _sec_asn1 ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/OSX/libsecurity_asn1/lib/*.h)
_sec_farm_dir(security_asn1 ${_sec_asn1})
file(GLOB _sec_der ${CMAKE_CURRENT_SOURCE_DIR}/../libDER/include/libDER/*.h)
_sec_farm_dir(security_libDER/libDER ${_sec_der})
file(GLOB _sec_sos ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/keychain/SecureObjectSync/*.h)
_sec_farm_dir(Security/SecureObjectSync ${_sec_sos})
file(GLOB _sec_cf ${PUREDARWIN_COREFOUNDATION_PREFIX}/include/*.h)
_sec_farm_dir(CoreFoundation ${_sec_cf})
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/EXTERNAL_HEADERS/AssertMacros.h
     ${_sec_farm}/AssertMacros.h SYMBOLIC)

_sec_farm_dir(bsm ${CMAKE_SOURCE_DIR}/src/Libraries/libdarwin/pd-compat-include/bsm/libbsm.h)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/src/Libraries/dyld/upstream/include/dlfcn.h
     ${_sec_farm}/dlfcn.h SYMBOLIC)


_sec_farm_dir(os
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/os/assumes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/compat-include/os/base_private.h)

add_darwin_shared_library(Security
    MACOSX_VERSION_MIN 26.5
    INSTALL_NAME /System/Library/Frameworks/Security.framework/Versions/A/Security)

# Apple ships these but PureDarwin does not build them:
#   SecCertificatePath.c, so_01_serverencryption.c, and iOSforOSX-SecAttr.c
# The rest need pieces we do not have yet: libsecurity_smime for CMS,
# libsecurity_keychain for import/export, SecureObjectSync, and AppleKeyStore.
set(_sec_skip_c
    SecCertificatePath.c so_01_serverencryption.c iOSforOSX-SecAttr.c
    SecItemBackup.c SecImportExport.c p12import.c
    SecSCEP.c SecCertificateRequest.c SecSharedCredential.c
    SecOTRSession.c SecOTRSessionAKE.c SecOTRPackets.c SecOTRPacketData.c
    SecOTRMath.c SecOTRDHKey.c SecOTRUtils.c
    SecOTRFullIdentity.c SecOTRPublicIdentity.c)

file(GLOB _sec_c
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/OSX/sec/Security/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/OSX/utilities/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/OSX/libsecurity_asn1/lib/*.c)
foreach(_f ${_sec_c})
    get_filename_component(_fn ${_f} NAME)
    if(NOT _fn IN_LIST _sec_skip_c)
        target_sources(Security PRIVATE ${_f})
    endif()
endforeach()

target_sources(Security PRIVATE
    ${_sec}/OSX/sec/ipc/client.c
    ${_sec}/sectask/SecTask.c

    # SecKey is ObjC and SecCertificate/SecPolicy/SecRSAKey/SecIdentity all call
    # into it, so there is no C-only Security. These are the ObjC sources whose
    # Foundation use PureDarwin can satisfy; the rest want NSSet, NSUUID,
    # NSOrderedSet, NSKeyedArchiver, NSUserDefaults and NSPropertyListSerialization.
    ${_sec}/OSX/sec/Security/SecKey.m
    ${_sec}/OSX/sec/Security/SecECKey.m
    ${_sec}/OSX/sec/Security/SecKeyAdaptors.m
    ${_sec}/OSX/sec/Security/SecAccessControl.m
    ${_sec}/OSX/sec/Security/SecCTKKey.m
    ${_sec}/OSX/sec/Security/SecItemRateLimit.m
    ${_sec}/OSX/utilities/SecKnownFilePaths.m
    ${_sec}/OSX/utilities/SecAutorelease.m
    ${_sec}/keychain/ot/OTConstants.m

    compat/pd_ctkclient.c
    compat/pd_coreauthd.c
    compat/pd_libaks_acl.c
    compat/pd_security_unvendored.c
)

target_include_directories(Security PRIVATE
    ${_sec_farm}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/OSX
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/OSX/sec
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/OSX/sec/Security
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/OSX/sec/ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/OSX/utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/base
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/OSX/libsecurity_asn1
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/OSX/libsecurity_asn1/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/securityd/securityd_service/KeyStore
    # debugging.h includes <asl.h>; SecItem.c and SecTrust.c <os/activity.h>;
    # SecPolicyLeafCallbacks.c <dlfcn.h>.
    ${CMAKE_SOURCE_DIR}/src/Libraries/syslog/libsystem_asl.tproj/include
    ${CMAKE_SOURCE_DIR}/src/Libraries/libsystem_trace/os-include
    # SecCFError.c includes <notify.h>.
    ${CMAKE_SOURCE_DIR}/src/Libraries/XPC/notify
    # SecTask.c includes <IOKit/IOKitLib.h> and <bsm/libbsm.h>.
    ${CMAKE_SOURCE_DIR}/src/Libraries/IOKit/iokituser/include
    ${PUREDARWIN_COREFOUNDATION_PREFIX}/include
    ${PUREDARWIN_SQLITE_PREFIX}/include
    ${PUREDARWIN_LIBOBJC_PREFIX}/usr/include
    ${PUREDARWIN_FOUNDATION_PREFIX}/usr/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/corecrypto/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../CommonCrypto/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../CommonCrypto/include/Private
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/libxpc/include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/headers/usr/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/darwin
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale/FreeBSD
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/stdtime/FreeBSD
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/src/BlocksRuntime
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/include/pthread
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/private_include
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libsystem_kernel/mach
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/iokit          # IOKit/IOReturn.h
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
)

target_compile_definitions(Security PRIVATE
    # The sec core is the iOS implementation; the types it needs
    # (SecCEBasicConstraints and friends) sit behind SEC_OS_IPHONE.
    SEC_IOS_ON_OSX=1
    TARGET_OS_OSX=1
    # Without this CoreFoundation selects its Swift CFSTR, which references
    # _CF_CONSTANT_OBJECT_STRONG_RC and breaks every file using a string constant.
    DEPLOYMENT_RUNTIME_OBJC=1
    # cc_config.h states that consumers set this; without it every ccrsa_/ccec_
    # context argument is a type error.
    CORECRYPTO_DONOT_USE_TRANSPARENT_UNION=1
    # TARGET_HAS_KEYSTORE defaults on for x86_64 because the test is
    # TARGET_CPU_X86 rather than TARGET_CPU_X86_64, and there is no AppleKeyStore.
    USE_KEYSTORE=0
    # Every in-tree library joining this build states the same libc feature
    # contract; libc-features.h hard-errors on a mismatch.
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
    PRIVATE=1
)

target_compile_options(Security PRIVATE
    -w
    -include ${CMAKE_CURRENT_SOURCE_DIR}/compat/pd_security_prefix.h
    # __FP__ is Carbon fp.h's own include guard. The ObjC sources reach it
    # through Foundation, and its __fpclassifyf returns long where the SDK's
    # math.h returns int. math.h already declares everything used.
    -D__FP__
    $<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>
)

target_link_options(Security PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-platform_version,macos,26.5,26.5
    "${PUREDARWIN_COREFOUNDATION_PREFIX}/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation"
    -Wl,-L,${PUREDARWIN_LIBOBJC_PREFIX}/usr/lib
    -lobjc
    -Wl,-L,${PUREDARWIN_FOUNDATION_PREFIX}/usr/lib
    -lFoundation
    -Wl,-L,${PUREDARWIN_SQLITE_PREFIX}/lib
    -lsqlite3
    -lSystem)

file(GLOB _sec_cc ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/corecrypto/src/*.c)
add_library(security_corecrypto OBJECT
    ${_sec_cc}
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/corecrypto/cc_user_stub.c)
target_include_directories(security_corecrypto PRIVATE
    $<TARGET_PROPERTY:Security,INCLUDE_DIRECTORIES>
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/corecrypto/private)
target_compile_definitions(security_corecrypto PRIVATE
    __DARWIN_ONLY_UNIX_CONFORMANCE=1 PRIVATE=1)
# ccdes.c falls off the end of a non-void function; clang 21 errors by default.
target_compile_options(security_corecrypto PRIVATE
    -w -nostdinc -fPIC -Wno-error=return-mismatch)
target_link_libraries(security_corecrypto PRIVATE
    AvailabilityHeaders clang_bootstrap_headers libc_public_headers
    libc_features pthread_common_headers)

# libcn's pd_cc_digest_bridge.c re-exports CC_MD5/CC_SHA* for consumers that do
# not build CommonDigest.c; here it would collide with it.
file(GLOB _sec_ccc
    ${CMAKE_CURRENT_SOURCE_DIR}/../CommonCrypto/lib/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../CommonCrypto/libcn/*.c)
list(FILTER _sec_ccc EXCLUDE REGEX "pd_cc_digest_bridge")
add_library(security_commoncrypto OBJECT ${_sec_ccc})
# CCCryptorReset_internal.h reaches for <mach-o/dyld.h>, which then wants
# <mach-o/loader.h> out of the tree's staged mach-o headers.
target_include_directories(security_commoncrypto PRIVATE
    $<TARGET_PROPERTY:Security,INCLUDE_DIRECTORIES>
    ${CMAKE_SOURCE_DIR}/src/Libraries/dyld/upstream/include
    ${CMAKE_BINARY_DIR}/generated-headers
    ${CMAKE_SOURCE_DIR}/src/Libraries/architecture/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../CommonCrypto/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../CommonCrypto/libcn
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/corecrypto/private)
target_compile_definitions(security_commoncrypto PRIVATE
    __DARWIN_ONLY_UNIX_CONFORMANCE=1 PRIVATE=1)
target_compile_options(security_commoncrypto PRIVATE -w -nostdinc -fPIC)
target_link_libraries(security_commoncrypto PRIVATE
    AvailabilityHeaders clang_bootstrap_headers libc_public_headers
    libc_features pthread_common_headers)

target_sources(Security PRIVATE
    $<TARGET_OBJECTS:security_corecrypto>
    $<TARGET_OBJECTS:security_commoncrypto>)

# libDER is likewise a static library inside libSystem, not re-exported.
target_link_libraries(Security PRIVATE
    libDER_static
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
)

add_dependencies(Security libc_features libSystem_B_stub dyld libsystem_kernel_static)

install(TARGETS Security
    DESTINATION System/Library/Frameworks/Security.framework/Versions/A
    COMPONENT BaseSystem)

install(DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/trust/headers/
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/keychain/headers/
    DESTINATION System/Library/Frameworks/Security.framework/Versions/A/Headers
    COMPONENT BaseSystem
    FILES_MATCHING PATTERN "*.h")

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/base/SecBase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/base/SecBasePriv.h
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/base/SecRandom.h
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/cssm/certextensions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/sectask/SecTask.h
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/sectask/SecTaskPriv.h
    ${CMAKE_CURRENT_SOURCE_DIR}/${_sec}/sectask/SecEntitlements.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Security/Security.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Security/Authorization.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Security/SecKeychain.h
    DESTINATION System/Library/Frameworks/Security.framework/Versions/A/Headers
    COMPONENT BaseSystem)
