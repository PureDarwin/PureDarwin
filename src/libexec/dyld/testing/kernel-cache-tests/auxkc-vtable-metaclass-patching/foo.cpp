
#include "foo.h"
#include <memory.h>

void* operator new(size_t size) { return (void*)1; }
void operator delete(void*) { }

OSDefineMetaClassAndStructors( Foo, OSObject )

OSMetaClassDefineReservedUnused( Foo, 0 )
OSMetaClassDefineReservedUnused( Foo, 1 )
OSMetaClassDefineReservedUnused( Foo, 2 )
OSMetaClassDefineReservedUnused( Foo, 3 )
