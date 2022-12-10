#!/bin/sh
BISON=$(dirname "$0")/bison
if [ ! -f "${BISON}" -o ! -x "${BISON}" ]; then
	BISON=/usr/bin/bison
fi
exec "${BISON}" -y "$@"
