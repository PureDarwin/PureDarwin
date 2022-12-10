/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

#import <Block.h>
#import <stdio.h>
#import <stdlib.h>
#import "test.h"

// TEST_CONFIG

int recovered = 0;
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
        
        void test(void);

	int version() CONST { return _version; }
private:
	mutable int _version;
};

TestObject::TestObject(CONST TestObject& inObj)
	
{
        ++constructors;
        _version = inObj._version;
	//printf("%p (%d) -- TestObject(const TestObject&) called", this, _version); 
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

#if 1
TestObject& TestObject::operator=(CONST TestObject& inObj)
{
	//printf("%p -- operator= called", this);
        _version = inObj._version;
	return *this;
}
#endif

void TestObject::test(void)  {
    void (^b)(void) = ^{ recovered = _version; };
    void (^b2)(void) = Block_copy(b);
    b2();
    Block_release(b2);
}



void testRoutine() {
    TestObject one;

    
    one.test();
}
    
    

int main() {
    testRoutine();
    if (recovered != 1) {
        fail("didn't recover byref block variable");
    }

    succeed(__FILE__);
}
