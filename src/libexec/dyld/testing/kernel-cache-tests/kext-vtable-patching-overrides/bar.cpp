
#include "foo.h"

class Bar : public FooSub
{
    OSDeclareDefaultStructors( Bar )
    
public:
    virtual int foo();
};

OSDefineMetaClassAndStructors( Bar, FooSub )

int Bar::foo() {
	return 1;
}

int bar() {
	Bar* bar = new Bar();
	return bar->foo();
}
