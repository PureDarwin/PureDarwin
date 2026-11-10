{ lib
, runCommand
, python3
, openssl
, cacert
, src
}:

# trustd's system trust store, /System/Library/Security/Certificates.bundle, built from Mozilla's roots.
# the escrow placeholder's private key goes to /dev/null as it is made, so it can never sign anything
let
  # the store version trustd reports, from the root set's own version (3.131.1 -> 3131001)
  parts = lib.splitString "." cacert.version;
  num = i: if builtins.length parts > i then lib.toInt (builtins.elemAt parts i) else 0;
  version = num 0 * 1000000 + num 1 * 1000 + num 2;
in
runCommand "puredarwin-certificates-bundle-${cacert.version}" {
  nativeBuildInputs = [ python3 openssl ];
} ''
  openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -keyout /dev/null \
    -subj "/O=PureDarwin/CN=PureDarwin escrow placeholder" -days 36500 -outform DER -out escrow.der
  python3 ${src}/gen-certificates-bundle.py ${cacert}/etc/ssl/certs/ca-bundle.crt \
    $out/System/Library/Security/Certificates.bundle ${toString version} escrow.der
''
