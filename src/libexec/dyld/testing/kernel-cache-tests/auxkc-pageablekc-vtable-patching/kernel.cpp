
#include "kernel.h"

OSDefineMetaClassAndStructors( KernelClass, OSObject )

// Index 0 has been replaced with a method
OSMetaClassDefineReservedUsed(KernelClass, 0)

int KernelClass::foo() {
	return 0;
}

int KernelClass::kernelClassUsed0() {
	return 0;
}
