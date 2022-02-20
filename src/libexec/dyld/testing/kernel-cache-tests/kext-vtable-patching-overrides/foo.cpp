
#include "foo.h"

OSDefineMetaClassAndStructors( Foo, OSObject )

// Index 0 has been replaced with a method
OSMetaClassDefineReservedUsed(Foo, 0)
OSMetaClassDefineReservedUnused( Foo, 1 )
OSMetaClassDefineReservedUnused( Foo, 2 )
OSMetaClassDefineReservedUnused( Foo, 3 )

int Foo::foo() {
	return 0;
}

int Foo::fooOverride() {
	return 0;
}

int Foo::fooUsed0() {
	return 0;
}

OSDefineMetaClassAndStructors( FooSub, Foo )

int FooSub::foo() {
	return 0;
}

int FooSub::fooOverride() {
	return 0;
}

int foo() {
	Foo* foo = new Foo();
	return foo->foo() + foo->fooUsed0();
}
