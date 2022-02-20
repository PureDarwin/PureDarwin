
#include "foo1.h"

OSDefineMetaClassAndStructors( Foo1, KernelClass )

// Index 0 has been replaced with a method
OSMetaClassDefineReservedUsed(Foo1, 0)
// Index 1 has been replaced with a method
OSMetaClassDefineReservedUsed( Foo1, 1 )

OSMetaClassDefineReservedUnused( Foo1, 2 )
OSMetaClassDefineReservedUnused( Foo1, 3 )

int Foo1::foo() {
	return 0;
}

int Foo1::foo1Used0() {
	return 0;
}

int Foo1::foo1Used1() {
	return 0;
}
