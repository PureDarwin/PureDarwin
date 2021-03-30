#!/bin/sh

set -e

cat $INPUT | fgrep CONFIG_DEFINES | sed -Ee 's,^export CONFIG_DEFINES = ,,' -e 's,-D,\
#define ,g' -e 's,=, ,g' -e 's,",,g' > $OUTPUT
