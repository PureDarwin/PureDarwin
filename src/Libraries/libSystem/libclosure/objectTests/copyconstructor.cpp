/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

#include <stdio.h>
#include <Block.h>
#include "test.h"

// TEST_CONFIG

// rdar://6243400,rdar://6289367


int constructors = 0;
int destructors = 0;


#define CONST const

class TestObject
{
public:
	TestObject(CONST TestObject& inObj);
	TestObject();
	~TestObject();
	
	TestObject& operator=(CONST TestObject& inObj);

	int version() CONST { return _version; }
private:
	mutable int _version;
};

TestObject::TestObject(CONST TestObject& inObj)
	
{
        ++constructors;
        _version = inObj._version;
	//printf("%p (%d) -- TestObject(const TestObject&) called\n", this, _version); 
}


TestObject::TestObject()
{
        _version = ++constructors;
	//printf("%p (%d) -- TestObject() called\n", this, _version); 
}


TestObject::~TestObject()
{
	//printf("%p -- ~TestObject() called\n", this);
        ++destructors;
}


TestObject& TestObject::operator=(CONST TestObject& inObj)
{
	//printf("%p -- operator= called\n", this);
        _version = inObj._version;
	return *this;
}



void testRoutine() {
    TestObject one;
    
    void (^b)(void) __unused = ^{ printf("my const copy of one is %d\n", one.version()); };
}
    
    

int main() {
    testRoutine();
    if (constructors == 0) {
        fail("No copy constructors!!!");
    }
    if (constructors != destructors) {
        fail("%d constructors but only %d destructors", constructors, destructors);
        return 1;
    }

    succeed(__FILE__);
}
