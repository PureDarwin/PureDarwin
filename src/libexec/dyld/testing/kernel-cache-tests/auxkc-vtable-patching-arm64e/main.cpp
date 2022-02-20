
#include <libkern/c++/OSMetaClass.h>
#include <Kernel/libkern/c++/OSObject.h>

int __cxa_pure_virtual = 0;

#if 0
class OSObject : public OSMetaClassBase
{
    OSDeclareAbstractStructors(OSObject);

public:
    OSMetaClassDeclareReservedUnused(OSObject, 0);
    OSMetaClassDeclareReservedUnused(OSObject, 1);
    OSMetaClassDeclareReservedUnused(OSObject, 2);
    OSMetaClassDeclareReservedUnused(OSObject, 3);
    OSMetaClassDeclareReservedUnused(OSObject, 4);
    OSMetaClassDeclareReservedUnused(OSObject, 5);
    OSMetaClassDeclareReservedUnused(OSObject, 6);
    OSMetaClassDeclareReservedUnused(OSObject, 7);
    OSMetaClassDeclareReservedUnused(OSObject, 8);
    OSMetaClassDeclareReservedUnused(OSObject, 9);
    OSMetaClassDeclareReservedUnused(OSObject, 10);
    OSMetaClassDeclareReservedUnused(OSObject, 11);
    OSMetaClassDeclareReservedUnused(OSObject, 12);
    OSMetaClassDeclareReservedUnused(OSObject, 13);
    OSMetaClassDeclareReservedUnused(OSObject, 14);
    OSMetaClassDeclareReservedUnused(OSObject, 15);
};
#endif

// OSDefineMetaClassAndAbstractStructors(OSObject, 0);
/* Class global data */
OSObject::MetaClass OSObject::gMetaClass;
const OSMetaClass * const OSObject::metaClass = &OSObject::gMetaClass;
const OSMetaClass * const OSObject::superClass = NULL;

// Virtual Padding
OSMetaClassDefineReservedUnused(OSObject, 0);
OSMetaClassDefineReservedUnused(OSObject, 1);
OSMetaClassDefineReservedUnused(OSObject, 2);
OSMetaClassDefineReservedUnused(OSObject, 3);
OSMetaClassDefineReservedUnused(OSObject, 4);
OSMetaClassDefineReservedUnused(OSObject, 5);
OSMetaClassDefineReservedUnused(OSObject, 6);
OSMetaClassDefineReservedUnused(OSObject, 7);
OSMetaClassDefineReservedUnused(OSObject, 8);
OSMetaClassDefineReservedUnused(OSObject, 9);
OSMetaClassDefineReservedUnused(OSObject, 10);
OSMetaClassDefineReservedUnused(OSObject, 11);
OSMetaClassDefineReservedUnused(OSObject, 12);
OSMetaClassDefineReservedUnused(OSObject, 13);
OSMetaClassDefineReservedUnused(OSObject, 14);
OSMetaClassDefineReservedUnused(OSObject, 15);

// struct IORPC { };

// OSMetaClassBase
OSMetaClassBase::OSMetaClassBase() { }
OSMetaClassBase::~OSMetaClassBase() { }
kern_return_t OSMetaClassBase::Dispatch(const IORPC rpc) { return 0; }
bool OSMetaClassBase::isEqualTo(const OSMetaClassBase*) const { return false; }

// OSMetaClass
OSMetaClass::OSMetaClass(const char* inClassName, const OSMetaClass* inSuperClass, unsigned int inClassSize) { }
OSMetaClass::~OSMetaClass() { }
void OSMetaClass::reservedCalled(int ind) const { }
void OSMetaClass::retain() const { }
void OSMetaClass::release() const { }
void OSMetaClass::release(int freeWhen) const { }
int OSMetaClass::getRetainCount() const { return 0; }
const OSMetaClass* OSMetaClass::getMetaClass() const { return NULL; }
void OSMetaClass::taggedRetain(const void * tag) const  { }
void OSMetaClass::taggedRelease(const void * tag) const  { }
void OSMetaClass::taggedRelease(const void * tag, const int freeWhen) const { }
bool OSMetaClass::serialize(OSSerialize * serializer) const { return false; }
void OSMetaClass::instanceConstructed() const { }

// OSMetaClass reserved methods
// Thee are none of these for arm64e

// OSObject::MetaClass
OSObject::MetaClass::MetaClass() : OSMetaClass("OSObject", OSObject::superClass, sizeof(OSObject)) { }
OSObject *OSObject::MetaClass::alloc() const { return NULL; }
kern_return_t OSObject::MetaClass::Dispatch(const IORPC rpc) { return 0; }

// OSObject
OSObject::OSObject(OSMetaClass const*) { }
OSObject::~OSObject() { }
const OSMetaClass* OSObject::getMetaClass() const { return NULL; }
void OSObject::free() { }
bool OSObject::init() { return false; }
void OSObject::retain() const { }
void OSObject::release() const { }
void OSObject::release(int freeWhen) const { }
int OSObject::getRetainCount() const { return 0; }
void OSObject::taggedRetain(const void * tag) const  { }
void OSObject::taggedRelease(const void * tag) const  { }
void OSObject::taggedRelease(const void * tag, const int freeWhen) const { }
bool OSObject::serialize(OSSerialize * serializer) const { return false; }
kern_return_t OSObject::Dispatch(const IORPC rpc) { return 0; }
void* OSObject::operator new(unsigned long) { return (void*)1; }
void OSObject::operator delete(void*, unsigned long) { return; }

__attribute__((section(("__TEXT_EXEC, __text"))))
extern "C" int _start() {
    return 0;
}