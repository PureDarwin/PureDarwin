
#include <Kernel/libkern/c++/OSMetaClass.h>
#include <Kernel/libkern/c++/OSObject.h>

class Foo : public OSObject
{
    OSDeclareDefaultStructors( Foo )
    
public:
    virtual int foo();
    virtual int fooOverride();
    
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

class FooSub : public Foo
{
	OSDeclareDefaultStructors( FooSub )

public:
	virtual int foo();

#ifdef FOO_OVERRIDE
	virtual int fooOverride();
#endif

};