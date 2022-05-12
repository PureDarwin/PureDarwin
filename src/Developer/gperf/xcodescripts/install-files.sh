set -e -x

OSV="$DSTROOT"/usr/local/OpenSourceVersions
OSL="$DSTROOT"/usr/local/OpenSourceLicenses
install -d -o root -g wheel -m 0755 "$OSV"
install -c -o root -g wheel -m 0644 \
        "$PROJECT_DIR"/gperf.plist \
        "$OSV"
install -d -o root -g wheel -m 0755 "$OSL"
install -c -o root -g wheel -m 0644 \
        "$PROJECT_DIR"/COPYING \
        "$OSL"/gperf.txt
