
// BUILD:  $CXX main.cxx  -o $BUILD_DIR/operator-new.exe

// RUN:  ./operator-new.exe

#include <new>

#import "test_support.h"

//
// This test case verifies that calling operator new[] in libstdc++.dylib
// will turn around and call operator new in this main exectuable
//

static void* myLastNewAllocation;
static void* myLastDelete;

// Note: this is not weak.  That is specifically suppported
void* operator new(size_t s) throw (std::bad_alloc)
{
  myLastNewAllocation = malloc(s);
  return myLastNewAllocation;
}

struct Foo {
    int  bytes[10];
};

// Note: this is weak and because it is in main executable should override OS
__attribute__((weak))
void operator delete(void* p) throw()
{
    myLastDelete = p;
    ::free(p);
}

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    // test that OS's operator new[] redirects to my operator new
    myLastNewAllocation = NULL;
    char* stuff = new char[24];
    if ( (void*)stuff != myLastNewAllocation ) {
        FAIL("system array allocator not redirected through my operator new");
    }

    // test that program uses my operator new
    myLastNewAllocation = NULL;
    Foo* foo = new Foo();
    if ( (void*)foo != myLastNewAllocation ) {
        FAIL("allocation not redirected though my operator new");
    }

    //
    delete foo;
    if ( (void*)foo != myLastDelete ) {
        FAIL("deallocation not redirected though my operator delete");
    }

    PASS("Success");
}


