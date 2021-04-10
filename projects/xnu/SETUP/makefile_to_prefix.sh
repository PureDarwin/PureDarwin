#!/bin/sh

set -e

if [ -z "$1" -o -z "$2" ]; then
    echo Two arguments must be provided. 1>&2
    exit 1
fi

cat $1 | fgrep CONFIG_DEFINES | sed -Ee 's,^export CONFIG_DEFINES = ,,' -e 's,-D,\
#define ,g' -e 's,=, ,g' -e 's,",,g' | awk '
NF == 2 {
    printf("%s 1\n", $0);
    next
}

// {
    print($0);
}
' > $2
