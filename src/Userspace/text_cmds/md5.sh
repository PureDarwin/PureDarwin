#!/bin/sh
# md5: the macOS md5 command over toybox md5sum, "MD5 (file) = hash", -q hash only, -r "hash file", -s string
quiet=0 rev=0 rc=0

out() {
	if [ $quiet = 1 ]; then echo "$1"
	elif [ $rev = 1 ]; then echo "$1 $2"
	else echo "MD5 ($2) = $1"
	fi
}

while getopts pqrs:t opt; do
	case $opt in
	q) quiet=1 ;;
	r) rev=1 ;;
	s) h=$(printf %s "$OPTARG" | md5sum | cut -d' ' -f1)
	   if [ $quiet = 1 ]; then echo "$h"; elif [ $rev = 1 ]; then echo "$h \"$OPTARG\""; else echo "MD5 (\"$OPTARG\") = $h"; fi
	   done_any=1 ;;
	p|t) ;;
	*) echo "usage: md5 [-pqrt] [-s string] [files ...]" >&2; exit 1 ;;
	esac
done
shift $((OPTIND - 1))

if [ $# = 0 ]; then
	[ -n "$done_any" ] && exit 0
	md5sum | cut -d' ' -f1
	exit
fi
for f in "$@"; do
	if h=$(md5sum < "$f" 2>/dev/null | cut -d' ' -f1) && [ -f "$f" -o -c "$f" -o -p "$f" ]; then
		out "$h" "$f"
	else
		echo "md5: $f: No such file or directory" >&2
		rc=1
	fi
done
exit $rc
