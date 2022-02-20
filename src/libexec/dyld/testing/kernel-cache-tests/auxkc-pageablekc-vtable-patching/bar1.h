
#include "foo2.h"

class Bar1 : public Foo2
{
    OSDeclareDefaultStructors( Bar1 )
    
public:
    virtual int foo() override;
};