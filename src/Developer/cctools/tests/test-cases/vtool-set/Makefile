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
	## test -set-build-version
	##

	# use -set-build-version to change an existing load command
	#   A. one arch, no flag
	${VTOOL} -set-build-version macos 1.2.3 4.5.6 -tool clang 7.8.9	     \
	    -o hello2 hello
	${VTOOL} -show hello2 | ${CHECK} -p SBVEXIST
#SBVEXIST:      hello2:
#SBVEXIST-NEXT: Load command \d+
#SBVEXIST-NEXT:      cmd LC_SOURCE_VERSION
#SBVEXIST-NEXT:  cmdsize 16
#SBVEXIST-NEXT:  version 0.0
#SBVEXIST-NEXT: Load command \d+
#SBVEXIST-NEXT:      cmd LC_BUILD_VERSION
#SBVEXIST-NEXT:  cmdsize 32
#SBVEXIST-NEXT: platform MACOS
#SBVEXIST-NEXT:    minos 1.2.3
#SBVEXIST-NEXT:      sdk 4.5.6
#SBVEXIST-NEXT:   ntools 1
#SBVEXIST-NEXT:     tool CLANG
#SBVEXIST-NEXT:  version 7.8.9

	#   B. one arch, w/ flag
	${VTOOL} -arch x86_64 -set-build-version macos 1.2.3 4.5.6	     \
	    -tool clang 7.8.9						     \
	    -o hello2 hello
	${VTOOL} -arch x86_64 -show hello2 2>&1 |			     \
	    ${CHECK} -p SBVEXIST_X86_64
	${VTOOL} -arch x86_64 -set-build-version macos 1.2.3 4.5.6	     \
	    -tool clang 7.8.9						     \
	    -o hello2 hello_fat
	${VTOOL} -arch x86_64 -show hello2 2>&1 |			     \
	    ${CHECK} -p SBVEXIST_X86_64
# SBVEXIST_X86_64:      hello2 (architecture x86_64):
# SBVEXIST_X86_64-NEXT: Load command \d+
# SBVEXIST_X86_64-NEXT:      cmd LC_SOURCE_VERSION
# SBVEXIST_X86_64-NEXT:  cmdsize 16
# SBVEXIST_X86_64-NEXT:  version 0.0
# SBVEXIST_X86_64-NEXT: Load command \d+
# SBVEXIST_X86_64-NEXT:      cmd LC_BUILD_VERSION
# SBVEXIST_X86_64-NEXT:  cmdsize 32
# SBVEXIST_X86_64-NEXT: platform MACOS
# SBVEXIST_X86_64-NEXT:    minos 1.2.3
# SBVEXIST_X86_64-NEXT:      sdk 4.5.6
# SBVEXIST_X86_64-NEXT:   ntools 1
# SBVEXIST_X86_64-NEXT:     tool CLANG
# SBVEXIST_X86_64-NEXT:  version 7.8.9

	${VTOOL} -arch x86_64h -set-build-version macos 1.2.3 4.5.6	     \
	    -tool clang 7.8.9						     \
	    -o hello2 hello_fat
	${VTOOL} -arch x86_64h -show hello2 2>&1 |			     \
	    ${CHECK} -p SBVEXIST_X86_64H
# SBVEXIST_X86_64H:      hello2 (architecture x86_64h):
# SBVEXIST_X86_64H-NEXT: Load command \d+
# SBVEXIST_X86_64H-NEXT:      cmd LC_SOURCE_VERSION
# SBVEXIST_X86_64H-NEXT:  cmdsize 16
# SBVEXIST_X86_64H-NEXT:  version 0.0
# SBVEXIST_X86_64H-NEXT: Load command \d+
# SBVEXIST_X86_64H-NEXT:      cmd LC_BUILD_VERSION
# SBVEXIST_X86_64H-NEXT:  cmdsize 32
# SBVEXIST_X86_64H-NEXT: platform MACOS
# SBVEXIST_X86_64H-NEXT:    minos 1.2.3
# SBVEXIST_X86_64H-NEXT:      sdk 4.5.6
# SBVEXIST_X86_64H-NEXT:   ntools 1
# SBVEXIST_X86_64H-NEXT:     tool CLANG
# SBVEXIST_X86_64H-NEXT:  version 7.8.9

	#   C. two arch, w/ and w/o flag
	${VTOOL} -arch x86_64 -arch x86_64h				     \
	    -set-build-version macos 1.2.3 4.5.6			     \
	    -tool clang 7.8.9						     \
	    -o hello2 hello_fat
	${VTOOL} -show hello2 2>&1 | ${CHECK} -p SBVEXIST_FAT
	${VTOOL} -set-build-version macos 1.2.3 4.5.6			     \
	    -tool clang 7.8.9						     \
	    -o hello2 hello_fat
	${VTOOL} -show hello2 2>&1 | ${CHECK} -p SBVEXIST_FAT
# SBVEXIST_FAT:      hello2 (architecture x86_64):
# SBVEXIST_FAT-NEXT: Load command \d+
# SBVEXIST_FAT-NEXT:      cmd LC_SOURCE_VERSION
# SBVEXIST_FAT-NEXT:  cmdsize 16
# SBVEXIST_FAT-NEXT:  version 0.0
# SBVEXIST_FAT-NEXT: Load command \d+
# SBVEXIST_FAT-NEXT:      cmd LC_BUILD_VERSION
# SBVEXIST_FAT-NEXT:  cmdsize 32
# SBVEXIST_FAT-NEXT: platform MACOS
# SBVEXIST_FAT-NEXT:    minos 1.2.3
# SBVEXIST_FAT-NEXT:      sdk 4.5.6
# SBVEXIST_FAT-NEXT:   ntools 1
# SBVEXIST_FAT-NEXT:     tool CLANG
# SBVEXIST_FAT-NEXT:  version 7.8.9
# SBVEXIST_FAT:      hello2 (architecture x86_64h):
# SBVEXIST_FAT-NEXT: Load command \d+
# SBVEXIST_FAT-NEXT:      cmd LC_SOURCE_VERSION
# SBVEXIST_FAT-NEXT:  cmdsize 16
# SBVEXIST_FAT-NEXT:  version 0.0
# SBVEXIST_FAT-NEXT: Load command \d+
# SBVEXIST_FAT-NEXT:      cmd LC_BUILD_VERSION
# SBVEXIST_FAT-NEXT:  cmdsize 32
# SBVEXIST_FAT-NEXT: platform MACOS
# SBVEXIST_FAT-NEXT:    minos 1.2.3
# SBVEXIST_FAT-NEXT:      sdk 4.5.6
# SBVEXIST_FAT-NEXT:   ntools 1
# SBVEXIST_FAT-NEXT:     tool CLANG
# SBVEXIST_FAT-NEXT:  version 7.8.9

	# use -set-build-version to add an additional load command
	${VTOOL} -set-build-version 6 10.20.30 40.50.60		     \
	    -tool clang 70.80.90 -tool ld 4.13.72 -o hello2 hello
	${VTOOL} -show hello2 | ${CHECK} -p SBVADD
#SBVADD:      hello2:
#SBVADD-NEXT: Load command \d+
#SBVADD-NEXT:       cmd LC_BUILD_VERSION
#SBVADD-NEXT:   cmdsize 32
#SBVADD-NEXT:  platform MACOS
#SBVADD-NEXT:     minos .*
#SBVADD-NEXT:       sdk .*
#SBVADD-NEXT:    ntools 1
#SBVADD-NEXT:      tool LD
#SBVADD-NEXT:   version .*
#SBVADD-NEXT: Load command \d+
#SBVADD-NEXT:       cmd LC_SOURCE_VERSION
#SBVADD-NEXT:   cmdsize 16
#SBVADD-NEXT:   version 0.0
#SBVADD-NEXT: Load command \d+
#SBVADD-NEXT:       cmd LC_BUILD_VERSION
#SBVADD-NEXT:   cmdsize 40
#SBVADD-NEXT:  platform MACCATALYST
#SBVADD-NEXT:     minos 10.20.30
#SBVADD-NEXT:       sdk 40.50.60
#SBVADD-NEXT:    ntools 2
#SBVADD-NEXT:      tool CLANG
#SBVADD-NEXT:   version 70.80.90
#SBVADD-NEXT:      tool LD
#SBVADD-NEXT:   version 4.13.72

	# use -set-build-version to replace other load command
	${VTOOL} -r -set-build-version 6 1.2.3 4.5.6 -tool clang 7.8.9   \
	    -o hello2 hello
	${VTOOL} -show hello2 | ${CHECK} -p SBVREP
# SBVREP:      hello2:
# SBVREP-NEXT: Load command \d+
# SBVREP-NEXT:       cmd LC_SOURCE_VERSION
# SBVREP-NEXT:   cmdsize 16
# SBVREP-NEXT:   version 0.0
# SBVREP-NEXT: Load command \d+
# SBVREP-NEXT:       cmd LC_BUILD_VERSION
# SBVREP-NEXT:   cmdsize 32
# SBVREP-NEXT:  platform MACCATALYST
# SBVREP-NEXT:     minos 1.2.3
# SBVREP-NEXT:       sdk 4.5.6
# SBVREP-NEXT:    ntools 1
# SBVREP-NEXT:      tool CLANG
# SBVREP-NEXT:   version 7.8.9

	# add two build version load commands
	${VTOOL} -r -set-build-version macos 1 1			     \
	    -set-build-version 6 2 2				     \
	    -o hello2 hello
	${VTOOL} -show-build hello2 | ${CHECK} -p SBVREP2
# SBVREP2:      hello2:
# SBVREP2-NEXT: Load command \d+
# SBVREP2-NEXT:       cmd LC_BUILD_VERSION
# SBVREP2-NEXT:   cmdsize 24
# SBVREP2-NEXT:  platform MACOS
# SBVREP2-NEXT:     minos 1.0
# SBVREP2-NEXT:       sdk 1.0
# SBVREP2-NEXT:    ntools 0
# SBVREP2-NEXT: Load command \d+
# SBVREP2-NEXT:       cmd LC_BUILD_VERSION
# SBVREP2-NEXT:   cmdsize 24
# SBVREP2-NEXT:  platform MACCATALYST
# SBVREP2-NEXT:     minos 2.0
# SBVREP2-NEXT:       sdk 2.0
# SBVREP2-NEXT:    ntools 0

	# use -set-build-version to add a platform and tool by number
	${VTOOL} -r -set-build-version 6 1.2.3 4.5.6 -tool 1 7.8.9	     \
	    -o hello2 hello
	${VTOOL} -show-build hello2 | ${CHECK} -p SBVNUM
# SBVNUM:      hello2:
# SBVNUM-NEXT: Load command \d+
# SBVNUM-NEXT:       cmd LC_BUILD_VERSION
# SBVNUM-NEXT:   cmdsize 32
# SBVNUM-NEXT:  platform MACCATALYST
# SBVNUM-NEXT:     minos 1.2.3
# SBVNUM-NEXT:       sdk 4.5.6
# SBVNUM-NEXT:    ntools 1
# SBVNUM-NEXT:      tool CLANG
# SBVNUM-NEXT:   version 7.8.9

	${VTOOL} -r -set-build-version 100 1.2.3 4.5.6 -tool 200 7.8.9	     \
	    -o hello2 hello
	${VTOOL} -show-build hello2 | ${CHECK} -p SBVNUM2
# SBVNUM2:      hello2:
# SBVNUM2-NEXT: Load command \d+
# SBVNUM2-NEXT:       cmd LC_BUILD_VERSION
# SBVNUM2-NEXT:   cmdsize 32
# SBVNUM2-NEXT:  platform 100
# SBVNUM2-NEXT:     minos 1.2.3
# SBVNUM2-NEXT:       sdk 4.5.6
# SBVNUM2-NEXT:    ntools 1
# SBVNUM2-NEXT:      tool 200
# SBVNUM2-NEXT:   version 7.8.9

	${VTOOL} -r -set-build-version 0xFF 1.2.3 4.5.6 -tool 0xE0 7.8.9     \
	    -o hello2 hello
	${VTOOL} -show-build hello2 | ${CHECK} -p SBVNUM3
# SBVNUM3:      hello2:
# SBVNUM3-NEXT: Load command \d+
# SBVNUM3-NEXT:       cmd LC_BUILD_VERSION
# SBVNUM3-NEXT:   cmdsize 32
# SBVNUM3-NEXT:  platform 255
# SBVNUM3-NEXT:     minos 1.2.3
# SBVNUM3-NEXT:       sdk 4.5.6
# SBVNUM3-NEXT:    ntools 1
# SBVNUM3-NEXT:      tool 224
# SBVNUM3-NEXT:   version 7.8.9

	##
	## test -set-build-tool
	##

 	# add a tool to an existing build command
	${VTOOL} -set-build-tool macos clang 999.9.9 -o hello2 hello
	${VTOOL} -show-build hello2 | ${CHECK} -p SBTADD
# SBTADD:      hello2:
# SBTADD-NEXT: Load command \d+
# SBTADD-NEXT:       cmd LC_BUILD_VERSION
# SBTADD-NEXT:   cmdsize 40
# SBTADD-NEXT:  platform MACOS
# SBTADD-NEXT:     minos .*
# SBTADD-NEXT:       sdk .*
# SBTADD-NEXT:    ntools 2
# SBTADD-NEXT:      tool LD
# SBTADD-NEXT:   version .*
# SBTADD-NEXT:      tool CLANG
# SBTADD-NEXT:   version 999.9.9

 	# add a tool to a new build command
	${VTOOL} -set-build-version 999 1 2				     \
	    -set-build-tool 999 8 3 -o hello2 hello
	${VTOOL} -show-build hello2 | ${CHECK} -p SBTADD2
# SBTADD2:     hello2:
# SBTADD2:           cmd LC_BUILD_VERSION
# SBTADD2:      platform MACOS
# SBTADD2:     Load command \d+
# SBTADD2-NEXT:       cmd LC_BUILD_VERSION
# SBTADD2-NEXT:   cmdsize 32
# SBTADD2-NEXT:  platform 999
# SBTADD2-NEXT:     minos 1.0
# SBTADD2-NEXT:       sdk 2.0
# SBTADD2-NEXT:    ntools 1
# SBTADD2-NEXT:      tool 8
# SBTADD2-NEXT:   version 3.0

	# try to add a tool to an unknown build version
	${VTOOL} -set-build-tool 999 8 3 -o hello2 hello 2>&1 |		     \
	    ${CHECK} -p SBTADDERR
# SBTADDERR: .*vtool error: no build verion load command found for platform #999
# SBTADDERR-NOT: usage: vtool .*

	${VTOOL} -set-build-tool ios 8 3 -o hello2 hello 2>&1 |		     \
	    ${CHECK} -p SBTADDERR2
# SBTADDERR2: .*vtool error: no build verion load command found for platform ios
# SBTADDERR2-NOT: usage: vtool .*

	##
	## test -set-version-min
	##

	# convert a build version to a min version, without assuming the
	# compiler produced a build version
	${VTOOL} -set-build-version macos 1 1 -o hello2 hello
	${VTOOL} -set-version-min macos 1 2 -o hello2 hello2
	${VTOOL} -show-build hello2 | ${CHECK} -p SVMSET
#SVMSET-NOT:  cmd LC_BUILD_VERSION
#SVMSET:      cmd LC_VERSION_MIN_MACOSX
#SVMSET-NEXT: cmdsize 16
#SVMSET-NEXT:   version 1.0
#SVMSET-NEXT:       sdk 2.0
#SVMSET-NOT:  cmd LC_VERSION_MIN_MACOSX
#SVMSET-NOT:  cmd LC_BUILD_VERSION

	# add a version min to an existing file
	${VTOOL} -set-build-version 6 1 2 -o hello2 hello
	${VTOOL} -set-version-min macos 3 4 -o hello2 hello2
	${VTOOL} -show-build hello2 | ${CHECK} -p SVMADD
#SVMADD:      cmd LC_BUILD_VERSION
#SVMADD:      cmd LC_VERSION_MIN_MACOSX
#SVMADD-NEXT: cmdsize 16
#SVMADD-NEXT:   version 3.0
#SVMADD-NEXT:       sdk 4.0

	# add all the version mins
	${VTOOL} -set-version-min macos 1 2				     \
	        -set-version-min ios 3 4				     \
		-set-version-min tvos 5 6				     \
		-set-version-min watchos 7 8				     \
		-o hello2 hello
	${VTOOL} -show-build hello2 | ${CHECK} -p SVMALL
#SVMALL-NOT: cmd LC_BUILD_VERSION
#SVMALL: cmd LC_VERSION_MIN_MACOSX
#SVMALL-NOT: cmd LC_BUILD_VERSION
#SVMALL: cmd LC_VERSION_MIN_IPHONEOS
#SVMALL-NOT: cmd LC_BUILD_VERSION
#SVMALL: cmd LC_VERSION_MIN_TVOS
#SVMALL-NOT: cmd LC_BUILD_VERSION
#SVMALL: cmd LC_VERSION_MIN_WATCHOS
#SVMALL-NOT: cmd LC_BUILD_VERSION

	# add a tool to an existing version min
	${VTOOL} -set-version-min macos 1 2 -o hello2 hello
	${VTOOL} -set-build-tool macos ld 4 -o hello2 hello2 2>&1 |	     \
	    ${CHECK} -p SVMTOOL1
#SVMTOOL1: .*vtool error: hello2 version min load commands do not support tool versions
#SVMTOOL1-NOT: usage: vtool .*

	# try to add a tool to a version min
	${VTOOL} -r -set-version-min macos 1 1				     \
	    -set-build-tool macos 8 3 -o hello2 hello 2>&1 |		     \
	    ${CHECK} -p SVMTOOL2
#SVMTOOL2: .*vtool error: version min load commands do not support tool versions
#SVMTOOL2-NOT: usage: vtool .*

	##
	## test -set-source-version
	##

	${VTOOL} -set-source-version 1.2.3.4.5 -o hello2 hello
	${VTOOL} -show-source hello2 | ${CHECK} -p SETSOURCE1
# SETSOURCE1:      hello2:
# SETSOURCE1-NEXT: Load command \d+
# SETSOURCE1-NEXT:       cmd LC_SOURCE_VERSION
# SETSOURCE1-NEXT:   cmdsize 16
# SETSOURCE1-NEXT:   version 1.2.3.4.5
# SETSOURCE1-NOT:        cmd LC_SOURCE_VERSION
	${VTOOL} -set-source-version 2.3.4.5.6 -o hello2 hello2
	${VTOOL} -show-source hello2 | ${CHECK} -p SETSOURCE2
# SETSOURCE2:      hello2:
# SETSOURCE2-NEXT: Load command \d+
# SETSOURCE2-NEXT:       cmd LC_SOURCE_VERSION
# SETSOURCE2-NEXT:   cmdsize 16
# SETSOURCE2-NEXT:   version 2.3.4.5.6
# SETSOURCE2-NOT:        cmd LC_SOURCE_VERSION

	##
	## test codesign warning
	##
	${CC} -arch x86_64 -o hello ${TESTROOT}/src/hello.c
	codesign -s - hello
	${VTOOL} -set-source-version 1 -o hello2 hello 2>&1 |		     \
	    ${CHECK} -p CODESIGN
# CODESIGN: .*vtool warning: code signature will be invalid for hello2

	echo PASS

clean:
	rm -rf hello hello_fat hello2
