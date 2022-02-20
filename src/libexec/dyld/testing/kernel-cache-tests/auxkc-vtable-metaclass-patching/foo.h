
#include "osobject.h"

class Foo : public OSObject
{
    OSDeclareDefaultStructors( Foo )
    
public:
	// Override from OSMetaClassBase to let us find this slot in the vtable
	virtual void placeholder() { }

    OSMetaClassDeclareReservedUnused(Foo, 0);
    OSMetaClassDeclareReservedUnused(Foo, 1);
    OSMetaClassDeclareReservedUnused(Foo, 2);
    OSMetaClassDeclareReservedUnused(Foo, 3);
};