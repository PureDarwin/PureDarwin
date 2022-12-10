
#include "foo.h"

class Bar : public Foo
{
    OSDeclareDefaultStructors( Bar )
    
public:
    virtual int foo();

    virtual int replaced_with_fooUsed0();
};

OSDefineMetaClassAndStructors( Bar, Foo )

int Bar::foo() {
	return 1;
}

int bar() {
	Bar* bar = new Bar();
	return bar->foo();
}
