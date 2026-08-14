{ lib
, mkPureDarwinBuild
, corefoundation
, libobjc
, foundation
, sqlite
, src
, pname ? "puredarwin-security"
# Set both together for the arm64 flavour, the same way DiskArbitration and
# SystemConfiguration select their architecture.
, puredarwinArch ? null
, arm64CrossToolchain ? null
}:

# Security.framework, built from Apple's Security-59754.120.12 sources by
# src/Libraries/Security/CMakeLists.txt. The framework layout (Versions/A,
# Headers, the Current/ symlinks and the flat /usr/lib/libSecurity.dylib every
# other PureDarwin library also gets) is assembled here, since CMake's install
# rules only place the dylib and headers.
(mkPureDarwinBuild ({
  inherit pname src;
  version = "59754.120.12";
  buildTargets = [ "Security" ];
  enableProjects = false;
  enableKernel = false;
  enableUserspace = false;
  installUserland = false;
  installKernel = false;
  extraCmakeFlags = [
    "-DPUREDARWIN_ENABLE_SECURITY=ON"
    "-DPUREDARWIN_COREFOUNDATION_PREFIX=${corefoundation}"
    "-DPUREDARWIN_LIBOBJC_PREFIX=${libobjc}"
    "-DPUREDARWIN_FOUNDATION_PREFIX=${foundation}"
    "-DPUREDARWIN_SQLITE_PREFIX=${sqlite}"
  ];
} // lib.optionalAttrs (puredarwinArch != null) {
  inherit puredarwinArch arm64CrossToolchain;
})).overrideAttrs (old: {
  installPhase = ''
    runHook preInstall

    fw="$out/System/Library/Frameworks/Security.framework"
    mkdir -p "$fw/Versions/A/Headers"
    cp build-nix/src/Libraries/Security/libSecurity.dylib \
      "$fw/Versions/A/Security"

    # Apple's own API headers, so consumers see the real Security API. Note
    # base/Security.h is deliberately not among them: its SEC_OS_OSX_INCLUDES
    # branch pulls the whole CDSA header set, which is not vendored - our
    # include/Security/Security.h is the umbrella over what does exist.
    cp src/Libraries/Security/apple/trust/headers/*.h \
       src/Libraries/Security/apple/keychain/headers/*.h \
       src/Libraries/Security/apple/base/SecBase.h \
       src/Libraries/Security/apple/base/SecBasePriv.h \
       src/Libraries/Security/apple/base/SecRandom.h \
       src/Libraries/Security/apple/cssm/certextensions.h \
       src/Libraries/Security/apple/sectask/SecTask.h \
       src/Libraries/Security/apple/sectask/SecTaskPriv.h \
       src/Libraries/Security/apple/sectask/SecEntitlements.h \
       src/Libraries/Security/include/Security/*.h \
       "$fw/Versions/A/Headers/"

    # Apple's public headers reference these from their #if SEC_OS_OSX blocks,
    # which a consumer takes because it does not define SEC_IOS_ON_OSX the way
    # this build does. They are declarations only - the CDSA, code-signing and
    # CMS implementations are not vendored, so calling into them fails at link
    # time, which is the honest outcome. Computed as the include closure of the
    # headers above; keep it that way if either set changes.
    cp src/Libraries/Security/apple/OSX/libsecurity_cssm/lib/cssmconfig.h \
       src/Libraries/Security/apple/OSX/libsecurity_cssm/lib/cssmtype.h \
       src/Libraries/Security/apple/OSX/libsecurity_cssm/lib/cssmerr.h \
       src/Libraries/Security/apple/OSX/libsecurity_cssm/lib/x509defs.h \
       src/Libraries/Security/apple/cssm/cssmapple.h \
       src/Libraries/Security/apple/OSX/libsecurity_codesigning/lib/CSCommon.h \
       src/Libraries/Security/apple/OSX/libsecurity_codesigning/lib/SecCode.h \
       src/Libraries/Security/apple/OSX/libsecurity_keychain/lib/SecAccess.h \
       src/Libraries/Security/apple/OSX/libsecurity_asn1/lib/SecAsn1Types.h \
       src/Libraries/Security/apple/CMS/SecCMS.h \
       src/Libraries/Security/apple/OSX/libsecurity_keychain/lib/SecKeychain.h \
       src/Libraries/Security/apple/OSX/libsecurity_keychain/lib/SecKeychainItem.h \
       src/Libraries/Security/apple/OSX/libsecurity_keychain/lib/SecTrustedApplication.h \
       "$fw/Versions/A/Headers/"

    # Apple's SecCertificatePriv.h includes <security_libDER/libDER/libDER.h>,
    # the spelling their build uses for libDER. Publish our clean-room libDER's
    # headers under that name so a consumer of the framework can resolve it;
    # the code itself is already linked into the framework.
    # Both spellings from one include root: Apple's headers use the
    # security_libDER/ prefix, libDER's own headers include each other plainly.
    mkdir -p "$out/include/security_libDER/libDER" "$out/include/libDER"
    cp src/Libraries/libDER/include/libDER/*.h "$out/include/security_libDER/libDER/"
    cp src/Libraries/libDER/include/libDER/*.h "$out/include/libDER/"

    ln -s A "$fw/Versions/Current"
    ln -s Versions/Current/Security "$fw/Security"
    ln -s Versions/Current/Headers "$fw/Headers"

    # Some consumers link -lSecurity against .../usr/lib rather than -F the
    # frameworks directory, as they do for CoreFoundation and IOKitCF.
    mkdir -p "$out/usr/lib"
    ln -s "../../System/Library/Frameworks/Security.framework/Versions/A/Security" \
      "$out/usr/lib/libSecurity.dylib"

    runHook postInstall
  '';

  dontFixup = true;

  meta = (old.meta or { }) // {
    description = "Security.framework from Apple's Security-59754.120.12 sources";
  };
})
