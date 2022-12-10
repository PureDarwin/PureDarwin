
#include "metaclass.h"
#include "osobject.h"

int __cxa_pure_virtual = 0;
void operator delete(void*) { }

OSMetaClassBase::~OSMetaClassBase() { }
void OSMetaClassBase::placeholder() { }
//void OSMetaClassBase::_RESERVEDOSMetaClassBase4() { }
//void OSMetaClassBase::_RESERVEDOSMetaClassBase5() { }
//void OSMetaClassBase::_RESERVEDOSMetaClassBase6() { }
//void OSMetaClassBase::_RESERVEDOSMetaClassBase7() { }
int OSMetaClassBase::metaclassBaseUsed4() { return 0; }
int OSMetaClassBase::metaclassBaseUsed5() { return 0; }
int OSMetaClassBase::metaclassBaseUsed6() { return 0; }
int OSMetaClassBase::metaclassBaseUsed7() { return 0; }

OSMetaClass::~OSMetaClass() { }
void OSMetaClass::_RESERVEDOSMetaClass0() { }
void OSMetaClass::_RESERVEDOSMetaClass1() { }
void OSMetaClass::_RESERVEDOSMetaClass2() { }
void OSMetaClass::_RESERVEDOSMetaClass3() { }
void OSMetaClass::_RESERVEDOSMetaClass4() { }
void OSMetaClass::_RESERVEDOSMetaClass5() { }
void OSMetaClass::_RESERVEDOSMetaClass6() { }
void OSMetaClass::_RESERVEDOSMetaClass7() { }

OSObject::OSObject(const OSMetaClass *) { }
OSObject::~OSObject() { }

// OSDefineMetaClassAndAbstractStructors(OSObject, 0);
/* Class global data */
OSObject::MetaClass OSObject::gMetaClass;
const OSMetaClass * const OSObject::metaClass = &OSObject::gMetaClass;
const OSMetaClass * const OSObject::superClass = NULL;

OSObject::MetaClass::MetaClass() { }
OSObject* OSObject::MetaClass::alloc() const { return NULL; }

__attribute__((section(("__HIB, __text"))))
extern "C" int _start() {
	return 0;
}