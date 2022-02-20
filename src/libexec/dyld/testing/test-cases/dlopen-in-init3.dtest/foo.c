#include "test_support.h"

extern int bar();
extern int bazIsInited();

int foo() {
	if ( bar() != 0 ) {
		return 1;
	}
	if ( bazIsInited() == 0 ) {
		FAIL("didn't init baz");
	}
	return 0;
}
