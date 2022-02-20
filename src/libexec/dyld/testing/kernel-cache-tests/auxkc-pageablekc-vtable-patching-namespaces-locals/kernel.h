
#include <Kernel/libkern/c++/OSMetaClass.h>
#include <Kernel/libkern/c++/OSObject.h>

namespace X {

class KernelClass : public OSObject
{
    OSDeclareDefaultStructors( KernelClass )
    
public:
    virtual int foo();
    
#ifdef KERNEL_USED
    OSMetaClassDeclareReservedUsed(KernelClass, 0);
    virtual int kernelClassUsed0();
#else
    OSMetaClassDeclareReservedUnused(KernelClass, 0);
#endif
};

} // namespace X