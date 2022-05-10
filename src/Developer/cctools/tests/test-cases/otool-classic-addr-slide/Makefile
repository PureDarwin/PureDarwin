# PLATFORM: MACOS
# Compile a test program, and disassemble it using some kind of slide.

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean

all:
	# compile a test program
	${CC} ${TESTROOT}/src/objc_hi.m -framework Foundation -o hello

	# test missing slide
	${OTOOLC} -t hello -addr_slide 2>&1 | ${CHECK} -p MISSING
	${OTOOLC} -t hello -addr_slide= 2>&1 | ${CHECK} -p MISSING
# MISSING: error: .* (amount to slide addresses when printing pointers)

	${OTOOLC} -t hello -addr_slide 9870000 | ${CHECK}
	${OTOOLC} -tv hello -addr_slide 9870000 | ${CHECK}
	${OTOOLC} -tV hello -addr_slide 9870000 | ${CHECK}
	${OTOOLC} -s __TEXT __unwind_info hello -addr_slide 9870000 | ${CHECK}
	${OTOOLC} -v -s __TEXT __unwind_info hello -addr_slide 9870000	     \
	  | ${CHECK}
	${OTOOLC} -o hello -addr_slide 9870000 | ${CHECK}
	${OTOOLC} -ov hello -addr_slide 9870000 | ${CHECK}
# CHECK: .*\d*0987\d*.*

	echo PASS

clean:
	rm -f hello
