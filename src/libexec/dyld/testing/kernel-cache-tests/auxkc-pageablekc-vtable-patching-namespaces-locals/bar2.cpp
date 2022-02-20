
#include "bar1.h"

using namespace X;

class Bar2 : public Bar1
{
    OSDeclareDefaultStructors( Bar2 )
    
public:
    virtual int foo() override;
};

OSDefineMetaClassAndStructors( Bar2, Bar1 )

int Bar2::foo() {
	return 1;
}
