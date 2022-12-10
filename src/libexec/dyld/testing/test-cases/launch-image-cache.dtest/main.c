
// BUILD:  $CC foo1.c -dynamiclib -install_name $RUN_DIR/libfoo1.dylib -o $BUILD_DIR/libfoo1.dylib
// BUILD:  $CC foo2.c -dynamiclib -install_name $RUN_DIR/libfoo2.dylib -o $BUILD_DIR/libfoo2.dylib
// BUILD:  $CC foo3.c -dynamiclib -install_name $RUN_DIR/libfoo3.dylib -o $BUILD_DIR/libfoo3.dylib
// BUILD:  $CC foo4.c -dynamiclib -install_name $RUN_DIR/libfoo4.dylib -o $BUILD_DIR/libfoo4.dylib
// BUILD:  $CC foo5.c -dynamiclib -install_name $RUN_DIR/libfoo5.dylib -o $BUILD_DIR/libfoo5.dylib
// BUILD:  $CC foo6.c -dynamiclib -install_name $RUN_DIR/libfoo6.dylib -o $BUILD_DIR/libfoo6.dylib
// BUILD:  $CC foo7.c -dynamiclib -install_name $RUN_DIR/libfoo7.dylib -o $BUILD_DIR/libfoo7.dylib
// BUILD:  $CC foo8.c -dynamiclib -install_name $RUN_DIR/libfoo8.dylib -o $BUILD_DIR/libfoo8.dylib
// BUILD:  $CC foo9.c -dynamiclib -install_name $RUN_DIR/libfoo9.dylib -o $BUILD_DIR/libfoo9.dylib
// BUILD:  $CC foo10.c -dynamiclib -install_name $RUN_DIR/libfoo10.dylib -o $BUILD_DIR/libfoo10.dylib
// BUILD:  $CC foo11.c -dynamiclib -install_name $RUN_DIR/libfoo11.dylib -o $BUILD_DIR/libfoo11.dylib
// BUILD:  $CC foo12.c -dynamiclib -install_name $RUN_DIR/libfoo12.dylib -o $BUILD_DIR/libfoo12.dylib
// BUILD:  $CC foo13.c -dynamiclib -install_name $RUN_DIR/libfoo13.dylib -o $BUILD_DIR/libfoo13.dylib
// BUILD:  $CC foo14.c -dynamiclib -install_name $RUN_DIR/libfoo14.dylib -o $BUILD_DIR/libfoo14.dylib
// BUILD:  $CC foo15.c -dynamiclib -install_name $RUN_DIR/libfoo15.dylib -o $BUILD_DIR/libfoo15.dylib
// BUILD:  $CC foo16.c -dynamiclib -install_name $RUN_DIR/libfoo16.dylib -o $BUILD_DIR/libfoo16.dylib
// BUILD:  $CC foo17.c -dynamiclib -install_name $RUN_DIR/libfoo17.dylib -o $BUILD_DIR/libfoo17.dylib
// BUILD:  $CC foo18.c -dynamiclib -install_name $RUN_DIR/libfoo18.dylib -o $BUILD_DIR/libfoo18.dylib
// BUILD:  $CC foo19.c -dynamiclib -install_name $RUN_DIR/libfoo19.dylib -o $BUILD_DIR/libfoo19.dylib
// BUILD:  $CC foo20.c -dynamiclib -install_name $RUN_DIR/libfoo20.dylib -o $BUILD_DIR/libfoo20.dylib
// BUILD:  $CC foo21.c -dynamiclib -install_name $RUN_DIR/libfoo21.dylib -o $BUILD_DIR/libfoo21.dylib
// BUILD:  $CC foo22.c -dynamiclib -install_name $RUN_DIR/libfoo22.dylib -o $BUILD_DIR/libfoo22.dylib
// BUILD:  $CC foo23.c -dynamiclib -install_name $RUN_DIR/libfoo23.dylib -o $BUILD_DIR/libfoo23.dylib
// BUILD:  $CC foo24.c -dynamiclib -install_name $RUN_DIR/libfoo24.dylib -o $BUILD_DIR/libfoo24.dylib
// BUILD:  $CC foo25.c -dynamiclib -install_name $RUN_DIR/libfoo25.dylib -o $BUILD_DIR/libfoo25.dylib
// BUILD:  $CC foo26.c -dynamiclib -install_name $RUN_DIR/libfoo26.dylib -o $BUILD_DIR/libfoo26.dylib
// BUILD:  $CC foo27.c -dynamiclib -install_name $RUN_DIR/libfoo27.dylib -o $BUILD_DIR/libfoo27.dylib
// BUILD:  $CC foo28.c -dynamiclib -install_name $RUN_DIR/libfoo28.dylib -o $BUILD_DIR/libfoo28.dylib
// BUILD:  $CC foo29.c -dynamiclib -install_name $RUN_DIR/libfoo29.dylib -o $BUILD_DIR/libfoo29.dylib
// BUILD:  $CC foo30.c -dynamiclib -install_name $RUN_DIR/libfoo30.dylib -o $BUILD_DIR/libfoo30.dylib
// BUILD:  $CC foo31.c -dynamiclib -install_name $RUN_DIR/libfoo31.dylib -o $BUILD_DIR/libfoo31.dylib
// BUILD:  $CC foo32.c -dynamiclib -install_name $RUN_DIR/libfoo32.dylib -o $BUILD_DIR/libfoo32.dylib

// BUILD:  $CC main.c -o $BUILD_DIR/launch-image-cache.exe $BUILD_DIR/libfoo1.dylib $BUILD_DIR/libfoo2.dylib $BUILD_DIR/libfoo3.dylib $BUILD_DIR/libfoo4.dylib $BUILD_DIR/libfoo5.dylib $BUILD_DIR/libfoo6.dylib $BUILD_DIR/libfoo7.dylib $BUILD_DIR/libfoo8.dylib $BUILD_DIR/libfoo9.dylib $BUILD_DIR/libfoo10.dylib $BUILD_DIR/libfoo11.dylib $BUILD_DIR/libfoo12.dylib $BUILD_DIR/libfoo13.dylib $BUILD_DIR/libfoo14.dylib $BUILD_DIR/libfoo15.dylib $BUILD_DIR/libfoo16.dylib $BUILD_DIR/libfoo17.dylib $BUILD_DIR/libfoo18.dylib $BUILD_DIR/libfoo19.dylib $BUILD_DIR/libfoo20.dylib $BUILD_DIR/libfoo21.dylib $BUILD_DIR/libfoo22.dylib $BUILD_DIR/libfoo23.dylib $BUILD_DIR/libfoo24.dylib $BUILD_DIR/libfoo25.dylib $BUILD_DIR/libfoo26.dylib $BUILD_DIR/libfoo27.dylib $BUILD_DIR/libfoo28.dylib $BUILD_DIR/libfoo29.dylib $BUILD_DIR/libfoo30.dylib $BUILD_DIR/libfoo31.dylib $BUILD_DIR/libfoo32.dylib

// RUN:  ./launch-image-cache.exe

// Generate these with:
// for i in `seq 1 32`; do ( echo "int foo$i() { return $i; };" > foo$i.c ); done
// for i in `seq 1 32`; do ( echo "extern int foo$i();" ); done
// for i in `seq 1 32`; do ( echo "if (failedCheck(foo$i(), $i)) return 0;" ); done
// for i in `seq 1 32`; do ( echo "\$CC foo$i.c -dynamiclib -install_name \$RUN_DIR/libfoo$i.dylib -o \$BUILD_DIR/libfoo$i.dylib" ); done
// for i in `seq 1 32`; do ( echo -n "\$BUILD_DIR/libfoo$i.dylib " ); done

#include <stdio.h>

#include "test_support.h"

extern int foo1();
extern int foo2();
extern int foo3();
extern int foo4();
extern int foo5();
extern int foo6();
extern int foo7();
extern int foo8();
extern int foo9();
extern int foo10();
extern int foo11();
extern int foo12();
extern int foo13();
extern int foo14();
extern int foo15();
extern int foo16();
extern int foo17();
extern int foo18();
extern int foo19();
extern int foo20();
extern int foo21();
extern int foo22();
extern int foo23();
extern int foo24();
extern int foo25();
extern int foo26();
extern int foo27();
extern int foo28();
extern int foo29();
extern int foo30();
extern int foo31();
extern int foo32();

void failedCheck(int i, int j) {
    if (i != j) {
        FAIL("expected %d but got %d", j, i);
    }
}

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    failedCheck(foo1(), 1);
    failedCheck(foo2(), 2);
    failedCheck(foo3(), 3);
    failedCheck(foo4(), 4);
    failedCheck(foo5(), 5);
    failedCheck(foo6(), 6);
    failedCheck(foo7(), 7);
    failedCheck(foo8(), 8);
    failedCheck(foo9(), 9);
    failedCheck(foo10(), 10);
    failedCheck(foo11(), 11);
    failedCheck(foo12(), 12);
    failedCheck(foo13(), 13);
    failedCheck(foo14(), 14);
    failedCheck(foo15(), 15);
    failedCheck(foo16(), 16);
    failedCheck(foo17(), 17);
    failedCheck(foo18(), 18);
    failedCheck(foo19(), 19);
    failedCheck(foo20(), 20);
    failedCheck(foo21(), 21);
    failedCheck(foo22(), 22);
    failedCheck(foo23(), 23);
    failedCheck(foo24(), 24);
    failedCheck(foo25(), 25);
    failedCheck(foo26(), 26);
    failedCheck(foo27(), 27);
    failedCheck(foo28(), 28);
    failedCheck(foo29(), 29);
    failedCheck(foo30(), 30);
    failedCheck(foo31(), 31);
    failedCheck(foo32(), 32);

    PASS("Success");
}

