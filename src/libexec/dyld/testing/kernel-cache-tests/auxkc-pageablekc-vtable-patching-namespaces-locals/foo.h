
#include "kernel.h"

namespace X {

class Foo1 : public KernelClass
{
    OSDeclareDefaultStructors( Foo1 )
    
public:
    virtual int foo();
    
#ifdef FOO1_USED
    OSMetaClassDeclareReservedUsed(Foo1, 0);
    virtual int foo1Used0();
#else
    OSMetaClassDeclareReservedUnused(Foo1, 0);
#endif
    OSMetaClassDeclareReservedUnused(Foo1, 1);
    OSMetaClassDeclareReservedUnused(Foo1, 2);
    OSMetaClassDeclareReservedUnused(Foo1, 3);
};

} // namespace X