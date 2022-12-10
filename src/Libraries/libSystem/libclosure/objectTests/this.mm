/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CFLAGS -framework Foundation

// rdar://6275956

#import <Foundation/Foundation.h>
#import <Block.h>
#define TEST_CALLS_OPERATOR_NEW
#import "test.h"

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

#define EQUAL 1
#if EQUAL	
	TestObject& operator=(CONST TestObject& inObj);
#endif
        void test(void);

	int version() CONST { return _version; }
private:
	mutable int _version;
};

TestObject::TestObject(CONST TestObject& inObj)
	
{
        ++constructors;
        _version = inObj._version;
	printf("%p (%d) -- TestObject(const TestObject&) called", this, _version); 
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

#if EQUAL
TestObject& TestObject::operator=(CONST TestObject& inObj)
{
	printf("%p -- operator= called", this);
        _version = inObj._version;
	return *this;
}
#endif

void TestObject::test(void)  {
    void (^b)(void) = ^{ recovered = this->_version; };
    void (^b2)(void) = [b copy];
    b2();
}

void testRoutine() {
    TestObject *one = new TestObject();
    
    void (^b)(void) = [^{ recovered = one->version(); } copy];
    b();
    [b release];
    delete one;
}
    
    

int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    testRoutine();
    [pool drain];

    if (recovered != 1) {
        fail("%s: *** didn't recover byref block variable");
    }

    succeed(__FILE__);
}
