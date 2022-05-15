#!/bin/sh
# Simple script to run the libclosure tests
# Note: to build the testing root, the makefile will ask to authenticate with sudo
# <rdar://problem/6456031> ER: option to not require extra privileges (-nosudo or somesuch)
# Use the RootsDirectory environment variable to direct the build to somewhere other than /tmp/

StartingDir="$PWD"
TestsDir="`dirname $0`/objectTests/"

cd "$TestsDir"
make libsystemroot roottests
XIT=$?
make clean
cd "$StartingDir"
exit $XIT
