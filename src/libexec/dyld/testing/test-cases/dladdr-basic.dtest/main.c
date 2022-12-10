
// BUILD:  $CC main.c            -o $BUILD_DIR/dladdr-basic.exe

// RUN:  ./dladdr-basic.exe

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <dlfcn.h> 
#include <mach-o/dyld_priv.h>

#include "test_support.h"

extern char** environ;

#if __has_feature(ptrauth_calls)
    #include <ptrauth.h>
#endif

int mydata  = 5;

int bar()
{
    return 2;
}

static int foo()
{
    return 3;
}

__attribute__((visibility("hidden"))) int hide()
{
    return 4;
}

static const void *stripPointer(const void *ptr) {
#if __has_feature(ptrauth_calls)
    return __builtin_ptrauth_strip(ptr, ptrauth_key_asia);
#else
    return ptr;
#endif
}

// checks global symbol
static void verifybar()
{
    Dl_info info;
    if ( dladdr(&bar, &info) == 0 ) {
        FAIL("dladdr(&bar, xx) failed");
    }
    if ( strcmp(info.dli_sname, "bar") != 0 ) {
        FAIL("dladdr()->dli_sname is \"%s\" instead of \"bar\"", info.dli_sname);
    }
    if ( info.dli_saddr != stripPointer(&bar) ) {
        FAIL("dladdr()->dli_saddr is not &bar");
    }
    if ( info.dli_fbase != dyld_image_header_containing_address(&bar) ) {
        FAIL("dladdr()->dli_fbase is not image that contains &bar");
    }
}

// checks local symbol
static void verifyfoo()
{
    Dl_info info;
    if ( dladdr(&foo, &info) == 0 ) {
        FAIL("dladdr(&foo, xx) failed");
    }
    if ( strcmp(info.dli_sname, "foo") != 0 ) {
        FAIL("dladdr()->dli_sname is \"%s\" instead of \"foo\"", info.dli_sname);
        exit(0);
    }
    if ( info.dli_saddr != stripPointer(&foo) ) {
        FAIL("dladdr()->dli_saddr is not &foo");
    }
    if ( info.dli_fbase != dyld_image_header_containing_address(&foo) ) {
        FAIL("dladdr()->dli_fbase is not image that contains &foo");
    }
}

// checks hidden symbol
static void verifyhide()
{
    Dl_info info;
    if ( dladdr(&hide, &info) == 0 ) {
        FAIL("dladdr(&hide, xx) failed");
    }
    if ( strcmp(info.dli_sname, "hide") != 0 ) {
        FAIL("dladdr()->dli_sname is \"%s\" instead of \"hide\"", info.dli_sname);
    }
    if ( info.dli_saddr != stripPointer(&hide) ) {
        FAIL("dladdr()->dli_saddr is not &hide");
    }
    if ( info.dli_fbase != dyld_image_header_containing_address(&hide) ) {
        FAIL("dladdr()->dli_fbase is not image that contains &hide");
    }
}

// checks dylib symbol
static void verifymalloc()
{
    Dl_info info;
    if ( dladdr(&malloc, &info) == 0 ) {
        FAIL("dladdr(&malloc, xx) failed");
    }
    if ( strcmp(info.dli_sname, "malloc") != 0 ) {
        FAIL("dladdr()->dli_sname is \"%s\" instead of \"malloc\"", info.dli_sname);
        exit(0);
    }
    if ( info.dli_saddr != stripPointer(&malloc) ) {
        FAIL("dladdr()->dli_saddr is not &malloc");
    }
    if ( info.dli_fbase != dyld_image_header_containing_address(&malloc) ) {
        FAIL("dladdr()->dli_fbase is not image that contains &malloc");
    }
}

// checks dylib data symbol
static void verifyenviron()
{
    Dl_info info;
    if ( dladdr(&environ, &info) == 0 ) {
        FAIL("dladdr(&environ, xx) failed");
    }
    if ( strcmp(info.dli_sname, "environ") != 0 ) {
        FAIL("dladdr()->dli_sname is \"%s\" instead of \"environ\"", info.dli_sname);
    }
    if ( info.dli_saddr != &environ ) {
        FAIL("dladdr()->dli_saddr is not &environ");
    }
    if ( info.dli_fbase != dyld_image_header_containing_address(&environ) ) {
        FAIL("dladdr()->dli_fbase is not image that contains &environ");
    }
}


// checks data symbol in main executable
static void verifymydata()
{
    Dl_info info;
    if ( dladdr(&mydata, &info) == 0 ) {
        FAIL("dladdr(&mydata, xx) failed");
    }
    if ( strcmp(info.dli_sname, "mydata") != 0 ) {
        FAIL("dladdr()->dli_sname is \"%s\" instead of \"mydata\"", info.dli_sname);
    }
    if ( info.dli_saddr != &mydata ) {
        FAIL("dladdr()->dli_saddr is not &mydata");
    }
    if ( info.dli_fbase != dyld_image_header_containing_address(&mydata) ) {
        FAIL("dladdr()->dli_fbase is not image that contains &mydata");
    }
}


// checks passing NULL for info parameter gracefully fails
static void verifyNULL()
{
    Dl_info info;
    if ( dladdr(&malloc, NULL) != 0 ) {
        FAIL("dladdr(&malloc, NULL) did not fail");
    }
    if ( dladdr(NULL, NULL) != 0 ) {
        FAIL("dladdr(NULL, NULL) did not fail");
    }
}

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    verifybar();
    verifyhide();
    verifyfoo();
    verifymalloc();
    verifyenviron();
    verifymydata();
    verifyNULL();

    PASS("Success");}

