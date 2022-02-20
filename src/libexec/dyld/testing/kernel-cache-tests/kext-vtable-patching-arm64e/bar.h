
#include "foo.h"

class Bar : public Foo
{
    OSDeclareDefaultStructors( Bar )
    
public:
    virtual int foo();
};
