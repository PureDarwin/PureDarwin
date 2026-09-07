{ stdenv
, lib
, src
}:

# Headers only: ApplicationServices is a pure umbrella over CoreGraphics and
# CoreText, with no code of its own. QuartzCore's public headers import it, so
# anything including <QuartzCore/CALayer.h> needs it on the include path.
stdenv.mkDerivation {
  pname = "puredarwin-applicationservices";
  version = "0.1";

  inherit src;

  dontConfigure = true;
  dontBuild = true;
  dontFixup = true;

  installPhase = ''
    runHook preInstall

    frameworkDir="$out/System/Library/Frameworks/ApplicationServices.framework"
    mkdir -p "$frameworkDir/Versions/A/Headers" \
      "$frameworkDir/Versions/A/Resources"
    cp ApplicationServices.h "$frameworkDir/Versions/A/Headers/"
    cp Info.plist "$frameworkDir/Versions/A/Resources/"

    ln -s A "$frameworkDir/Versions/Current"
    ln -s Versions/Current/Headers "$frameworkDir/Headers"
    ln -s Versions/Current/Resources "$frameworkDir/Resources"

    mkdir -p "$out/usr/include"
    ln -s ../../System/Library/Frameworks/ApplicationServices.framework/Versions/A/Headers \
      "$out/usr/include/ApplicationServices"

    runHook postInstall
  '';

  meta = with lib; {
    description = "ApplicationServices umbrella headers (CoreGraphics + CoreText)";
    platforms = platforms.unix;
  };
}
