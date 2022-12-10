
#include <Kernel/libkern/c++/OSMetaClass.h>
#include <Kernel/libkern/c++/OSObject.h>

// Redefine this just so that we can write tests
#undef OSMetaClassDeclareReservedUnused
#define OSMetaClassDeclareReservedUnused(className, index)        \
    private:                                                      \
    virtual void _RESERVED ## className ## index ()

class Foo : public OSObject
{
    OSDeclareDefaultStructors( Foo )
    
public:
    virtual int foo();
    
#ifdef FOO_USED
    OSMetaClassDeclareReservedUsed(Foo, 0);
    virtual int fooUsed0();
#else
    OSMetaClassDeclareReservedUnused(Foo, 0);
#endif
    OSMetaClassDeclareReservedUnused(Foo, 1);
    OSMetaClassDeclareReservedUnused(Foo, 2);
    OSMetaClassDeclareReservedUnused(Foo, 3);
};