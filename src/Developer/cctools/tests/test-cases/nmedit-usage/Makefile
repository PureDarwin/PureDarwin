# PLATFORM: MACOS
#
# Verify nmedit and strip have different usage strings. I.e., that they aren't
# the same tool (even though they are).

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

NUSAGE="$(shell echo `${NMEDIT} -h 2>&1 |sed 's/nmedit/tool/'`)"

all:
	nusage="$(shell echo `${NMEDIT} -h 2>&1 | sed 's/nmedit/tool/'`)";   \
	susage="$(shell echo `${STRIP} -h 2>&1 | sed 's/strip/tool/'`)";     \
	if [ "$$nusage" == "$$susage" ]; then				     \
	  echo FAIL;							     \
	else								     \
	  echo PASS;							     \
	fi

clean:
	