# PLATFORM: MACOS
#
# This test verifies libtool will make an archive if subtype feature flags
# are set. This test tries to be correct without making assumptions about
# the default architecture for the platform, but will most likely produce
# .o and .a files that aren't sensible.

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

REWRITE = ${TESTROOT}/bin/rewrite-macho.pl

.PHONY: all clean

all:
	#
	# make an object file for the default architecture. verify we
	# can make an archive from that object file.
	#
	${CC} -arch $(ARCH) -o foo.o -c ${TESTROOT}/src/foo.c
	${LIBTOOL} -static -arch_only $(ARCH) -o libfoo.a foo.o
	${OTOOLC} -av libfoo.a | ${CHECK} -p LIBFOO
# LIBFOO: Archive : libfoo.a
# LIBFOO: .* __.SYMDEF SORTED
# LIBFOO: .* foo.o

	#
	# create new object files:
	#   foo2.o - Mach-O object with a modified cpusubtype
	#   foo3.o - LLVM Bitcode
	#   foo4.o - LLVM Bitcode with a modified cputype (if possible)
	subtype=`otool -h foo.o | tail -1 | awk '{print $$3}'`;		     \
	subtype=$$(($$subtype | 0xFF000000));				     \
	echo ${REWRITE} foo.o -v -c :$$subtype -o foo.o;		     \
	${REWRITE} foo.o -v -c :$$subtype -o foo2.o

	${CC} -arch $(ARCH) -o foo3.o -flto -c ${TESTROOT}/src/foo.c

	#
	# modify the object file's cpusubtype by adding some capability flags,
	# verify we can make a library from that file.
	#
	${LIBTOOL} -static -arch_only $(ARCH) -o libfoo2.a foo2.o
	${OTOOLC} -av libfoo2.a | ${CHECK} -p LIBFOO2
# LIBFOO2: Archive : libfoo2.a
# LIBFOO2: .* __.SYMDEF SORTED
# LIBFOO2: .* foo2.o

	#
	# add add both architectures to the same library.
	#
	${LIBTOOL} -static -arch_only $(ARCH) -o libfoo3.a foo.o foo2.o foo3.o
	${OTOOLC} -av libfoo3.a | ${CHECK} -p LIBFOO3
# LIBFOO3: Archive : libfoo3.a
# LIBFOO3: .* __.SYMDEF
# LIBFOO3: .* foo.o
# LIBFOO3: .* foo2.o
# LIBFOO3: .* foo3.o

	#
	# add add both architectures to the same library old-skuul.
	#
	$(AR) -ruS libfoo4.a foo.o foo2.o foo3.o 2>&1
	$(RANLIB) libfoo4.a
	${OTOOLC} -av libfoo4.a | ${CHECK} -p LIBFOO4
# LIBFOO4: Archive : libfoo4.a
# LIBFOO4: .* __.SYMDEF
# LIBFOO4: .* foo.o
# LIBFOO4: .* foo2.o
# LIBFOO4: .* foo3.o

	echo PASS

clean:
	rm -f foo*.o libfoo*.a

