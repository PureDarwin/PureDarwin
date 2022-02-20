
#include "foo2.h"

namespace X {

class Bar1 : public X::Foo2
{
    OSDeclareDefaultStructors( Bar1 )
    
public:
    virtual int foo() override;
};

} // namespace X