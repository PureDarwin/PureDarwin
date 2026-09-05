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

let
  # Shared with apple-sdk-pinned.nix so the SDK publishes the same headers.
  secHeaders = import ../../lib/security-headers.nix;
  secHeaderArgs = lib.concatStringsSep " " (
    map (d: "src/Libraries/Security/${d}/*.h") secHeaders.dirs
    ++ map (f: "src/Libraries/Security/${f}") (secHeaders.apiFiles ++ secHeaders.closureFiles));
in

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

    # Header set comes from nix/lib/security-headers.nix; see there for why
    # base/Security.h is excluded and what the second group is for.
    cp ${secHeaderArgs} "$fw/Versions/A/Headers/"

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
