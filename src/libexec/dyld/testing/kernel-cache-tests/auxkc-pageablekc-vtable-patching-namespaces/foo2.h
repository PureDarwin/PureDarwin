
#include "foo1.h"

namespace X {

class Foo2 : public Foo1
{
    OSDeclareDefaultStructors( Foo2 )
    
public:
    virtual int foo() override;

    virtual int foo1Used0() override;
};

} // namespace X