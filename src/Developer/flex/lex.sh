#!/bin/sh
FLEX=$(dirname "$0")/flex
if [ ! -f "${FLEX}" -o ! -x "${FLEX}" ]; then
    FLEX=/usr/bin/flex
fi
if [ "${COMMAND_MODE:=Unix2003}" != "legacy" ]; then
    exec "${FLEX}" -X ${1+"$@"}
else
    exec "${FLEX}" ${1+"$@"}
fi
