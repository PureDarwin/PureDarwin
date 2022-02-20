
// BUILD:  $CC main.c            -o $BUILD_DIR/crt-vars-libSystem.exe

// RUN:  ./crt-vars-libSystem.exe

#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <crt_externs.h>
#include <mach-o/ldsyms.h>

#include "test_support.h"

// This struct is passed as fifth parameter to libSystem.dylib's initializer so it record
// the address of crt global variables.
struct ProgramVars
{
    const void*        mh;
    int*               NXArgcPtr;
    char***            NXArgvPtr;
    char***            environPtr;
    char**             __prognamePtr;
};


// global variables defeined in crt1.o
extern char**  NXArgv;
extern int     NXArgc;
extern char**  environ;
extern char*   __progname;


static const struct ProgramVars* sVars;

void __attribute__((constructor))
myInit(int argc, const char* argv[], const char* envp[], const char* apple[], const struct ProgramVars* vars)
{
    sVars = vars;
}

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    if ( _NSGetArgv() != &NXArgv ) {
        FAIL("_NSGetArgv() != &NXArgv (%p!=%p) for %s", _NSGetArgv(), &NXArgv, argv[0]);
    }

    if ( _NSGetArgc() != &NXArgc ) {
        FAIL("_NSGetArgc() != &NXArgc (%p!=%p) for %s", _NSGetArgc(), &NXArgc, argv[0]);
    }

    if ( _NSGetEnviron() != &environ ) {
        FAIL("_NSGetEnviron() != &environv (%p!=%p) for %s", _NSGetEnviron(), &environ, argv[0]);
    }

    if ( _NSGetProgname() != &__progname ) {
        FAIL("_NSGetProgname() != &__progname (%p!=%p) for %s", _NSGetProgname(), &__progname, argv[0]);
    }

    if ( _NSGetMachExecuteHeader() != &_mh_execute_header ) {
        FAIL("_NSGetMachExecuteHeader() != &_mh_execute_headerv (%p!=%p) for %s", _NSGetMachExecuteHeader(), &_mh_execute_header, argv[0]);
    }

    if ( sVars->NXArgvPtr != &NXArgv ) {
        FAIL("sVars->NXArgvPtr != &NXArg (%p!=%p) for %s", sVars->NXArgvPtr, &NXArgv, argv[0]);
    }

    if ( sVars->NXArgcPtr != &NXArgc ) {
        FAIL("sVars->NXArgcPtr != &NXArgc (%p!=%p) for %s", sVars->NXArgcPtr, &NXArgc, argv[0]);
    }

    if ( sVars->environPtr != &environ ) {
        FAIL("sVars->environPtr != &environ (%p!=%p) for %s", sVars->environPtr, &environ, argv[0]);
    }

    if ( sVars->__prognamePtr != &__progname ) {
        FAIL("sVars->__prognamePtr != &__progname (%p!=%p) for %s", sVars->__prognamePtr, &__progname, argv[0]);
    }

    if ( sVars->mh != &_mh_execute_header ) {
        FAIL("sVars->mh != &_mh_execute_header (%p!=%p) for %s", sVars->mh, &_mh_execute_header, argv[0]);
    }
    PASS("Success");
}

