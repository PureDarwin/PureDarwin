
#include "bar.h"

class Baz : public Bar
{
    OSDeclareDefaultStructors( Baz )
    
public:
    virtual int foo();
};

OSDefineMetaClassAndStructors( Baz, Bar )

int Baz::foo() {
	return 1;
}

int baz() {
	Baz* baz = new Baz();
	return baz->foo();
}
