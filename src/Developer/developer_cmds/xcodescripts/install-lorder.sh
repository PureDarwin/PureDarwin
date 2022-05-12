set -ex

BINDIR="$DSTROOT"/"$DT_TOOLCHAIN_DIR"/usr/bin

install -c -o root -g wheel -m 0755 \
	"$PROJECT_DIR"/lorder/lorder.sh \
	"$BINDIR"/lorder
