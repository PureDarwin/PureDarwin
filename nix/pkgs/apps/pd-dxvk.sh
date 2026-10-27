#!/bin/sh
# DXVK for Wine prefixes on PureDarwin: Direct3D 8 to 11 over Vulkan. The DLLs go into a prefix the way DXVK's own
# setup_dxvk.sh puts them, and single programs can keep Wine's own Direct3D (Steam's client does, its web helper not)
set -e

dxvk=/usr/lib/dxvk
dlls="d3d8 d3d9 d3d10core d3d11 dxgi"
prefix=${WINEPREFIX:-$HOME/.wine}

usage() {
	cat <<EOF
usage: pd-dxvk install            DXVK into \$WINEPREFIX ($prefix) for every program
       pd-dxvk uninstall          back to Wine's own Direct3D
       pd-dxvk builtin-for EXE... these programs keep Wine's Direct3D (Steam: steam.exe)
       pd-dxvk native-for EXE...  only these programs use DXVK, when it is not installed for every program
       pd-dxvk status
EOF
	exit 1
}

overrides() {
	key=$1 value=$2
	for d in $dlls; do
		if [ -n "$value" ]; then
			wine reg add "$key" /v "$d" /d "$value" /f >/dev/null 2>&1
		else
			wine reg delete "$key" /v "$d" /f >/dev/null 2>&1 || true
		fi
	done
}

copy_dlls() {
	from=$1 to=$2
	[ -d "$to" ] || return 0
	for d in $dlls; do
		# the original is Wine's own stub, kept so uninstall can put it back
		[ -f "$to/$d.dll" ] && [ ! -f "$to/$d.dll.wine" ] && mv "$to/$d.dll" "$to/$d.dll.wine"
		cp "$from/$d.dll" "$to/$d.dll"
	done
}

restore_dlls() {
	to=$1
	[ -d "$to" ] || return 0
	for d in $dlls; do
		[ -f "$to/$d.dll.wine" ] && mv -f "$to/$d.dll.wine" "$to/$d.dll"
	done
}

[ -d "$prefix/drive_c" ] || { echo "pd-dxvk: no Wine prefix at $prefix (run wineboot first)" >&2; exit 1; }
export WINEPREFIX="$prefix" WINEDEBUG=-all

case "$1" in
install)
	copy_dlls "$dxvk/x64" "$prefix/drive_c/windows/system32"
	copy_dlls "$dxvk/x32" "$prefix/drive_c/windows/syswow64"
	overrides 'HKCU\Software\Wine\DllOverrides' native,builtin
	echo "pd-dxvk: installed into $prefix"
	;;
uninstall)
	restore_dlls "$prefix/drive_c/windows/system32"
	restore_dlls "$prefix/drive_c/windows/syswow64"
	overrides 'HKCU\Software\Wine\DllOverrides' ""
	wineboot -u >/dev/null 2>&1
	echo "pd-dxvk: removed from $prefix"
	;;
builtin-for|native-for)
	mode=builtin
	[ "$1" = native-for ] && mode=native,builtin
	shift
	[ $# -gt 0 ] || usage
	if [ "$mode" != builtin ]; then
		copy_dlls "$dxvk/x64" "$prefix/drive_c/windows/system32"
		copy_dlls "$dxvk/x32" "$prefix/drive_c/windows/syswow64"
	fi
	for exe in "$@"; do
		overrides "HKCU\\Software\\Wine\\AppDefaults\\$exe\\DllOverrides" "$mode"
		echo "pd-dxvk: $exe uses ${mode%%,*} Direct3D"
	done
	;;
status)
	if cmp -s "$dxvk/x64/d3d11.dll" "$prefix/drive_c/windows/system32/d3d11.dll"; then
		echo "pd-dxvk: DXVK files are in $prefix"
	else
		echo "pd-dxvk: Wine's own Direct3D in $prefix"
	fi
	wine reg query 'HKCU\Software\Wine\DllOverrides' 2>/dev/null | grep -iE "$(echo $dlls | tr ' ' '|')" || true
	;;
*)
	usage
	;;
esac
