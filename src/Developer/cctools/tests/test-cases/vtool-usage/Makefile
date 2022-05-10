# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile
FILE = /bin/ls

all:
	##
	## test default usage
	##

	${VTOOL} 2>&1 | ${CHECK} -p USAGE
# USAGE: usage: vtool.*

	# test usage error
	${VTOOL} -aaa 2>&1 | ${CHECK} -p UNKNOWN_OPTION
# UNKNOWN_OPTION:      error: unknown option: -aaa
# UNKNOWN_OPTION-NEXT: usage: vtool.*

	${VTOOL} -arch 2>&1 | ${CHECK} -p MISSING_ARCH
# MISSING_ARCH:      error: missing arch
# MISSING_ARCH-NEXT: usage: vtool.*

	${VTOOL} -arch xyz 2>&1 | ${CHECK} -p UNKNOWN_ARCH
# UNKNOWN_ARCH:      error: unknown arch: xyz
# UNKNOWN_ARCH-NEXT: usage: vtool.*

	${VTOOL} $(FILE) 2>&1 | ${CHECK} -p MISSING_CMD
# MISSING_CMD:      error: a -show, -set, or -remove command must be specified
# MISSING_CMD-NEXT: usage: vtool.*

	##
	## test show usage
	##

	${VTOOL} -show 2>&1 | ${CHECK} -p MISSING_IN
	${VTOOL} -show-build 2>&1 | ${CHECK} -p MISSING_IN
	${VTOOL} -show-source 2>&1 | ${CHECK} -p MISSING_IN
#MISSING_IN:      error: one input file must be specified
#MISSING_IN-NEXT: usage: vtool.*

	${VTOOL} -show -o output $(FILE) 2>&1 | ${CHECK} -p SHOWOUT
	${VTOOL} -show-build -o output $(FILE) 2>&1 | ${CHECK} -p SHOWOUT
	${VTOOL} -show-source -o output $(FILE) 2>&1 | ${CHECK} -p SHOWOUT
#SHOWOUT:      error: -show commands do not write output
#SHOWOUT-NEXT: usage: vtool .*

	##
	## test set-build-version usage
	##

	${VTOOL} -set-build-version 2>&1 | ${CHECK} -p SBVERR1
#SBVERR1:      error: missing -set-build-version platform
#SBVERR1-NEXT: usage: vtool .*

	${VTOOL} -set-build-version maco 2>&1 | ${CHECK} -p UNKNOWN_PLATFORM
#UNKNOWN_PLATFORM:      error: unknown platform: maco
#UNKNOWN_PLATFORM-NEXT: usage: vtool .*

	${VTOOL} -set-build-version macos 2>&1 | ${CHECK} -p SBVERR3
#SBVERR3:      error: missing -set-build-version min_version
#SBVERR3-NEXT: usage: vtool .*

	${VTOOL} -set-build-version macos 1 2>&1 | ${CHECK} -p SBVERR4
#SBVERR4:      error: missing -set-build-version sdk_version
#SBVERR4-NEXT: usage: vtool .*

	${VTOOL} -set-build-version macos 1 1 2>&1 | ${CHECK} -p MISSING_IN

	${VTOOL} -set-build-version macos 1 1 -tool 2>&1 | ${CHECK}	     \
	    -p SBVERR6
#SBVERR6:      error: missing -set-build-version -tool tool
#SBVERR6-NEXT: usage: vtool .*

	${VTOOL} -set-build-version macos 1 1 -tool clan 2>&1 | ${CHECK}     \
	    -p UNKNOWN_TOOL
#UNKNOWN_TOOL:      error: unknown tool: clan
#UNKNOWN_TOOL-NEXT: usage: vtool .*

	${VTOOL} -set-build-version macos 1 1 -tool clang 2>&1 | ${CHECK}    \
	    -p SBVERR8
#SBVERR8:      error: missing -set-build-version -tool version
#SBVERR8-NEXT: usage: vtool .*

	${VTOOL} -set-build-version macos 1 1 -tool clang 1 2>&1 |	     \
	    ${CHECK} -p MISSING_IN
	${VTOOL} -set-build-version macos 1 1 -tool clang 1 $(FILE) 2>&1 |   \
	    ${CHECK} -p MISSING_OUT
#MISSING_OUT:      error: one output file must be specified
#MISSING_OUT-NEXT: usage: vtool .*

	##
	## test set-build-tool usage
	##

	${VTOOL} -set-build-tool 2>&1 | ${CHECK} -p SBTERR1
# SBTERR1:      error: missing -set-build-tool platform
# SBTERR1-NEXT: usage: vtool .*

	${VTOOL} -set-build-tool maco 2>&1 | ${CHECK} -p UNKNOWN_PLATFORM

	${VTOOL} -set-build-tool macos 2>&1 | ${CHECK} -p SBTERR3
# SBTERR3:      error: missing -set-build-tool tool
# SBTERR3-NEXT: usage: vtool .*

	${VTOOL} -set-build-tool macos clan 2>&1 | ${CHECK} -p UNKNOWN_TOOL

	${VTOOL} -set-build-tool macos clang 2>&1 | ${CHECK} -p SBTERR5
# SBTERR5:      error: missing -set-build-tool version
# SBTERR5-NEXT: usage: vtool .*

	${VTOOL} -set-build-tool macos clang 1 2>&1 |			     \
	    ${CHECK} -p MISSING_IN
	${VTOOL} -set-build-tool macos clang 1 $(FILE) 2>&1 |		     \
	    ${CHECK} -p MISSING_OUT

	##
	## test set-version-min usage
	##

	${VTOOL} -set-version-min 2>&1 | ${CHECK} -p SVMERR1
#SVMERR1:      error: missing -set-version-min platform
#SVMERR1-NEXT: usage: vtool .*

	${VTOOL} -set-version-min maco 2>&1 | ${CHECK} -p UNKNOWN_PLATFORM

	${VTOOL} -set-version-min 6 2>&1 | ${CHECK} -p SVMERR3
#SVMERR3:      error: version min unsupported for platform: 6
#SVMERR3-NEXT: usage: vtool .*

	${VTOOL} -set-version-min macos 2>&1 | ${CHECK} -p SVMERR4
#SVMERR4:      error: missing -set-version-min min_version
#SVMERR4-NEXT: usage: vtool .*

	${VTOOL} -set-version-min macos 1 2>&1 | ${CHECK} -p SVMERR5
#SVMERR5:      error: missing -set-version-min sdk_version
#SVMERR5-NEXT: usage: vtool .*

	${VTOOL} -set-version-min macos 1 1 2>&1 | ${CHECK} -p MISSING_IN

	${VTOOL} -set-version-min macos 1 1 -tool 2>&1 | ${CHECK}	     \
	    -p SVMERR7
#SVMERR7:      error: -tool cannot be used with -set-version-min
#SVMERR7-NEXT: usage: vtool .*

	${VTOOL} -set-version-min 123 2>&1 | ${CHECK} -p SVMERR8
#SVMERR8:      error: version min unsupported for platform: 123
#SVMERR8-NEXT: usage: vtool .*

	##
	## test -remove-build-version usage
	##

	${VTOOL} -remove-build-version 2>&1 | ${CHECK} -p RBVERR1
#RBVERR1:      error: missing -remove-build-version platform
#RBVERR1-NEXT: usage: vtool .*

	${VTOOL} -remove-build-version maco 2>&1 |			     \
	    ${CHECK} -p UNKNOWN_PLATFORM
	${VTOOL} -remove-build-version macos 2>&1 |			     \
	    ${CHECK} -p MISSING_IN
	${VTOOL} -remove-build-version macos $(FILE) 2>&1 |		     \
	    ${CHECK} -p MISSING_OUT

	##
	## test -remove-build-tool usage
	##

	${VTOOL} -remove-build-tool 2>&1 | ${CHECK} -p RBTERR1
#RBTERR1:      error: missing -remove-build-tool platform
#RBTERR1-NEXT: usage: vtool .*

	${VTOOL} -remove-build-tool maco 2>&1 | ${CHECK} -p UNKNOWN_PLATFORM

	${VTOOL} -remove-build-tool macos 2>&1 | ${CHECK} -p RBTERR3
#RBTERR3:      error: missing -remove-build-tool tool
#RBTERR3-NEXT: usage: vtool .*

	${VTOOL} -remove-build-tool macos clan 2>&1 |			     \
	    ${CHECK} -p UNKNOWN_TOOL
	${VTOOL} -remove-build-tool macos clang 2>&1 |			     \
	    ${CHECK} -p MISSING_IN
	${VTOOL} -remove-build-tool macos clang $(FILE) 2>&1 |		     \
	    ${CHECK} -p MISSING_OUT

	##
	## test -remove-source-version usage
	##

	${VTOOL} -remove-source-version 2>&1 | ${CHECK} -p MISSING_IN
	${VTOOL} -remove-source-version $(FILE) 2>&1 | ${CHECK} -p MISSING_OUT

	echo PASS

clean:

