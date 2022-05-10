# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# compile a test program
	#
	# ideally we'd use the ${ARCH} and ${VALID_ARCHS} flags from
	# common.makefile, but check.pl doesn't currently allow variable
	# expansion in CHECK comments. So we'll hardcode arch values for now.
	#
	${CC} -arch x86_64 -o hello ${TESTROOT}/src/hello.c
	${CC} -arch x86_64 -arch x86_64h -o hello_fat ${TESTROOT}/src/hello.c

	##
	## test common -show errors
	##

	${VTOOL} -show nada 2>&1 | ${CHECK} -p NOSUCHFILE
	${VTOOL} -show-build nada 2>&1 | ${CHECK} -p NOSUCHFILE
	${VTOOL} -show-source nada 2>&1 | ${CHECK} -p NOSUCHFILE
#NOSUCHFILE: .* error: nada: can't open file: No such file or directory
#NOSUCHFILE-NOT: usage: vtool.*

	##
	## test -show
	##

	${VTOOL} -show hello 2>&1 | ${CHECK} -p SHOW
#SHOW:      hello:
#SHOW-NEXT: Load command \d+
#SHOW-NEXT:       cmd LC_BUILD_VERSION
#SHOW-NEXT:   cmdsize 32
#SHOW-NEXT:  platform MACOS
#SHOW-NEXT:     minos .*
#SHOW-NEXT:       sdk .*
#SHOW-NEXT:    ntools 1
#SHOW-NEXT:      tool LD
#SHOW-NEXT:   version .*
#SHOW:      Load command \d+
#SHOW-NEXT:       cmd LC_SOURCE_VERSION
#SHOW-NEXT:   cmdsize 16
#SHOW-NEXT:   version 0.0

	${VTOOL} -arch x86_64 -show hello 2>&1 |			     \
	    ${CHECK} -p SHOW_ARCH_X86_64
	${VTOOL} -arch x86_64 -show hello_fat 2>&1 |			     \
	    ${CHECK} -p SHOW_ARCH_X86_64
#SHOW_ARCH_X86_64:      hello\(_fat\)? (architecture x86_64):
#SHOW_ARCH_X86_64-NEXT: Load command \d+
#SHOW_ARCH_X86_64-NEXT:       cmd LC_BUILD_VERSION
#SHOW_ARCH_X86_64-NEXT:   cmdsize 32
#SHOW_ARCH_X86_64-NEXT:  platform MACOS
#SHOW_ARCH_X86_64-NEXT:     minos .*
#SHOW_ARCH_X86_64-NEXT:       sdk .*
#SHOW_ARCH_X86_64-NEXT:    ntools 1
#SHOW_ARCH_X86_64-NEXT:      tool LD
#SHOW_ARCH_X86_64-NEXT:   version .*
#SHOW_ARCH_X86_64:      Load command \d+
#SHOW_ARCH_X86_64-NEXT:       cmd LC_SOURCE_VERSION
#SHOW_ARCH_X86_64-NEXT:   cmdsize 16
#SHOW_ARCH_X86_64-NEXT:   version 0.0

	${VTOOL} -arch x86_64h -show hello_fat 2>&1 |			     \
	    ${CHECK} -p SHOW_ARCH_X86_64H
#SHOW_ARCH_X86_64H:      hello_fat (architecture x86_64h):
#SHOW_ARCH_X86_64H-NEXT: Load command \d+
#SHOW_ARCH_X86_64H-NEXT:       cmd LC_BUILD_VERSION
#SHOW_ARCH_X86_64H-NEXT:   cmdsize 32
#SHOW_ARCH_X86_64H-NEXT:  platform MACOS
#SHOW_ARCH_X86_64H-NEXT:     minos .*
#SHOW_ARCH_X86_64H-NEXT:       sdk .*
#SHOW_ARCH_X86_64H-NEXT:    ntools 1
#SHOW_ARCH_X86_64H-NEXT:      tool LD
#SHOW_ARCH_X86_64H-NEXT:   version .*
#SHOW_ARCH_X86_64H:      Load command \d+
#SHOW_ARCH_X86_64H-NEXT:       cmd LC_SOURCE_VERSION
#SHOW_ARCH_X86_64H-NEXT:   cmdsize 16
#SHOW_ARCH_X86_64H-NEXT:   version 0.0

	${VTOOL} -show hello_fat 2>&1 |					     \
	    ${CHECK} -p SHOW_FAT
	${VTOOL} -arch x86_64 -arch x86_64h -show hello_fat 2>&1 |	     \
	    ${CHECK} -p SHOW_FAT
# SHOW_FAT:      hello_fat (architecture x86_64):
# SHOW_FAT-NEXT: Load command \d+
# SHOW_FAT-NEXT:       cmd LC_BUILD_VERSION
# SHOW_FAT-NEXT:   cmdsize 32
# SHOW_FAT-NEXT:  platform MACOS
# SHOW_FAT-NEXT:     minos .*
# SHOW_FAT-NEXT:       sdk .*
# SHOW_FAT-NEXT:    ntools 1
# SHOW_FAT-NEXT:      tool LD
# SHOW_FAT-NEXT:   version .*
# SHOW_FAT:      Load command \d+
# SHOW_FAT-NEXT:       cmd LC_SOURCE_VERSION
# SHOW_FAT-NEXT:   cmdsize 16
# SHOW_FAT-NEXT:   version 0.0
# SHOW_FAT:      hello_fat (architecture x86_64h):
# SHOW_FAT-NEXT: Load command \d+
# SHOW_FAT-NEXT:       cmd LC_BUILD_VERSION
# SHOW_FAT-NEXT:   cmdsize 32
# SHOW_FAT-NEXT:  platform MACOS
# SHOW_FAT-NEXT:     minos .*
# SHOW_FAT-NEXT:       sdk .*
# SHOW_FAT-NEXT:    ntools 1
# SHOW_FAT-NEXT:      tool LD
# SHOW_FAT-NEXT:   version .*
# SHOW_FAT:      Load command \d+
# SHOW_FAT-NEXT:       cmd LC_SOURCE_VERSION
# SHOW_FAT-NEXT:   cmdsize 16
# SHOW_FAT-NEXT:   version 0.0

	${VTOOL} -arch i386 -show hello 2>&1 |				     \
	    ${CHECK} -p BAD_ARCH
# BAD_ARCH: .*vtool error: hello file does not contain architecture: i386
# BAD_ARCH-NOT: usage: vtool.*

	##
	## test -show-build
	##

	${VTOOL} -show-build hello 2>&1 | ${CHECK} -p SHOWBUILD
#SHOWBUILD:      hello:
#SHOWBUILD-NEXT: Load command \d+
#SHOWBUILD-NEXT:       cmd LC_BUILD_VERSION
#SHOWBUILD-NEXT:   cmdsize 32
#SHOWBUILD-NEXT:  platform MACOS
#SHOWBUILD-NEXT:     minos .*
#SHOWBUILD-NEXT:       sdk .*
#SHOWBUILD-NEXT:    ntools 1
#SHOWBUILD-NEXT:      tool LD
#SHOWBUILD-NEXT:   version .*
#SHOWBUILD-NOT:       cmd LC_SOURCE_VERSION

	##
	## test -show-source
	##

	${VTOOL} -show-source hello 2>&1 | ${CHECK} -p SHOWSOURCE
#SHOWSOURCE:      hello:
#SHOWSOURCE-NEXT: Load command \d+
#SHOWSOURCE-NEXT:       cmd LC_SOURCE_VERSION
#SHOWSOURCE-NEXT:   cmdsize 16
#SHOWSOURCE-NEXT:   version 0.0
#SHOWSOURCE-NOT:        cmd LC_BUILD_VERSION

	${VTOOL} -show-space hello 2>&1 | ${CHECK} -p SHOWSPACE
#SHOWSPACE:      hello:
#SHOWSPACE-NEXT:   Mach header size:     32
#SHOWSPACE-NEXT:   Load command size: \s*\d+
#SHOWSPACE-NEXT:   Available space: \s*\d+
#SHOWSPACE-NEXT:   Total: \s*\d+

	echo PASS

clean:
	rm -rf hello hello_fat
