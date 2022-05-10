# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

LIBS =	libFoo.dylib							     \
	libFooPath.dylib						     \
	libFooSuffix.dylib						     \
	libFooPathSuffix.dylib						     \
	libFooVers.dylib						     \
	libFooPathVers.dylib						     \
	libx.dylib							     \
	libxSuffix.dylib						     \
	libxPathSuffix.dylib						     \
	libATSVersSuffix.dylib						     \
	libATSPathVersSuffix.dylib					     \
	QT.dylib							     \
	QTPath.dylib							     \
	libfoo_bar.dylib						     \
	libfoo_barSuffix.dylib

OBJS = 	main.o								     \
	foo.o								     \
	fooSuffix.o							     \
	fooPath.o							     \
	fooPathSuffix.o							     \
	fooVers.o							     \
	fooPathVers.o							     \
	x.o								     \
        xSuffix.o							     \
        xPathSuffix.o							     \
	atsVersSuffix.o							     \
	atsPathVersSuffix.o						     \
	qt.o								     \
	qtPath.o							     \
	foo_bar.o							     \
	foo_barSuffix.o

all: main
	${NMC} -m main | $(CHECK) -v
# CHECK: 0000000100000000 (__TEXT,__text) [referenced dynamically] external __mh_execute_header
# CHECK:                  (undefined) external _atsPathVersSuffix (from libPathATS)
# CHECK:                  (undefined) external _atsVersSuffix (from libATS)
# CHECK:                  (undefined) external _foo (from Foo)
# CHECK:                  (undefined) external _fooPath (from FooPath)
# CHECK:                  (undefined) external _fooPathSuffix (from FooPathSuffix)
# CHECK:                  (undefined) external _fooPathVers (from FooPathVers)
# CHECK:                  (undefined) external _fooSuffix (from FooSuffix)
# CHECK:                  (undefined) external _fooVers (from FooVers)
# CHECK:                  (undefined) external _foo_bar (from libfoo_bar)
# CHECK:                  (undefined) external _foo_barSuffix (from libfoo_barSuffix)
# CHECK: \[\[:xdigit:\]\]+ (__TEXT,__text) external _main
# CHECK:                  (undefined) external _qt (from QT)
# CHECK:                  (undefined) external _qtPath (from QTPath)
# CHECK:                  (undefined) external _x (from libx)
# CHECK:                  (undefined) external _xPathSuffix (from libxPathSuffix)
# CHECK:                  (undefined) external _xSuffix (from libxSuffix)
# CHECK:                  (undefined) external dyld_stub_binder (from libSystem)

	echo PASS

main: main.o $(LIBS)
	$(CC) -o $@ main.o $(LIBS)

libFoo.dylib: foo.o
	$(CC) -dynamiclib -o $@ foo.o -install_name			     \
	Foo.framework/Foo

libFooSuffix.dylib: fooSuffix.o
	$(CC) -dynamiclib -o $@ fooSuffix.o -install_name		     \
	FooSuffix.framework/FooSuffix_debug

libFooPath.dylib: fooPath.o
	$(CC) -dynamiclib -o $@ fooPath.o -install_name			     \
	/System/Library/Frameworks/FooPath.framework/FooPath

libFooPathSuffix.dylib: fooPathSuffix.o
	$(CC) -dynamiclib -o $@ fooPathSuffix.o -install_name		     \
	/System/Library/Frameworks/FooPathSuffix.framework/FooPathSuffix_profile

libFooVers.dylib: fooVers.o
	$(CC) -dynamiclib -o $@ fooVers.o -install_name			     \
	FooVers.framework/Versions/A/FooVers

libFooPathVers.dylib: fooPathVers.o
	$(CC) -dynamiclib -o $@ fooPathVers.o -install_name		     \
	/System/Library/Frameworks/FooPathVers.framework/Versions/B/FooPathVers

libx.dylib: x.o
	$(CC) -dynamiclib -o $@ x.o -install_name			     \
	libx.dylib

libxSuffix.dylib: xSuffix.o
	$(CC) -dynamiclib -o $@ xSuffix.o -install_name			     \
	libxSuffix_profile.dylib

libxPathSuffix.dylib: xPathSuffix.o
	$(CC) -dynamiclib -o $@ xPathSuffix.o -install_name		     \
	/usr/local/lib/libxPathSuffix_debug.dylib

libATSVersSuffix.dylib: atsVersSuffix.o
	$(CC) -dynamiclib -o $@ atsVersSuffix.o -install_name		     \
	libATS.A_profile.dylib

libATSPathVersSuffix.dylib: atsPathVersSuffix.o
	$(CC) -dynamiclib -o $@ atsPathVersSuffix.o -install_name	     \
	/usr/lib/libPathATS.A_profile.dylib

QT.dylib: qt.o
	$(CC) -dynamiclib -o $@ qt.o -install_name			     \
	QT.A.qtx

QTPath.dylib: qtPath.o
	$(CC) -dynamiclib -o $@ qtPath.o -install_name			     \
	/lib/QTPath.qtx

libfoo_bar.dylib: foo_bar.o
	$(CC) -dynamiclib -o $@ foo_bar.o -install_name			     \
	/usr/lib/libfoo_bar.dylib

libfoo_barSuffix.dylib: foo_barSuffix.o
	$(CC) -dynamiclib -o $@ foo_barSuffix.o -install_name		     \
	/usr/lib/libfoo_barSuffix_profile.dylib

clean:
	rm -f main $(OBJS) $(LIBS)

