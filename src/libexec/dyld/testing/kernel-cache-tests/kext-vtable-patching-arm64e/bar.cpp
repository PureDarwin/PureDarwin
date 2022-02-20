
#include "bar.h"

OSDefineMetaClassAndStructors( Bar, Foo )

int Bar::foo() {
	return 1;
}

int bar() {
	Bar* bar = new Bar();
	return bar->foo();
}
