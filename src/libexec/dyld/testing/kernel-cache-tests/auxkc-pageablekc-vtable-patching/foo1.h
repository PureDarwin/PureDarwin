
#include "kernel.h"

class Foo1 : public KernelClass
{
    OSDeclareDefaultStructors( Foo1 )
    
public:
    virtual int foo() override;
    
#ifdef FOO1_USED0
    OSMetaClassDeclareReservedUsed(Foo1, 0);
    virtual int foo1Used0();
#else
    OSMetaClassDeclareReservedUnused(Foo1, 0);
#endif

#ifdef FOO1_USED1
    OSMetaClassDeclareReservedUsed(Foo1, 1);
    virtual int foo1Used1();
#else
    OSMetaClassDeclareReservedUnused(Foo1, 1);
#endif

    OSMetaClassDeclareReservedUnused(Foo1, 2);
    OSMetaClassDeclareReservedUnused(Foo1, 3);
};