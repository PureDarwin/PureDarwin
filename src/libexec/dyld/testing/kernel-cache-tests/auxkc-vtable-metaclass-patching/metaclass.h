
#ifndef METACLASS_H
#define METACLASS_H

#define NULL 0
#define APPLE_KEXT_OVERRIDE

#define OSDeclareCommonStructors(className, dispatch)           \
    private:                                                    \
    static const OSMetaClass * const superClass;                \
    public:                                                     \
    static const OSMetaClass * const metaClass;                 \
	static class MetaClass : public OSMetaClass {           \
	public:                                                 \
	    MetaClass();                                        \
	    virtual OSObject *alloc() const APPLE_KEXT_OVERRIDE;\
	} gMetaClass;                                           \
	friend class className ::MetaClass;                     \
    protected:                                                  \
    className (const OSMetaClass *);                            \
    virtual ~ className () APPLE_KEXT_OVERRIDE;

#define _OSDeclareAbstractStructors(className, dispatch)                        \
    OSDeclareCommonStructors(className, dispatch);                              \
    private:                                                                    \
    className (void); /* Make primary constructor private in abstract */            \
    protected:

#define OSDeclareAbstractStructorsWithDispatch(className)                       \
_OSDeclareAbstractStructors(className, dispatch)

#define OSMetaClassDefineReservedUsed(className, index)
#define OSMetaClassDefineReservedUnused(className, index)       \
void className ::_RESERVED ## className ## index ()  { }

#define OSMetaClassDeclareReservedUsed(className, index)
#define OSMetaClassDeclareReservedUnused(className, index)        \
    private:                                                      \
    virtual void _RESERVED ## className ## index ()

#define _OSDeclareDefaultStructors(className, dispatch)    \
    OSDeclareCommonStructors(className, dispatch);        \
    public:                                     \
    className (void);                           \
    protected:

#define OSDeclareDefaultStructors(className)   \
_OSDeclareDefaultStructors(className, )

#define OSDefineMetaClassWithInit(className, superclassName, init)            \
	/* Class global data */                                                   \
    className ::MetaClass className ::gMetaClass;                             \
    const OSMetaClass * const className ::metaClass =                         \
	& className ::gMetaClass;                                             \
    const OSMetaClass * const className ::superClass =                        \
	& superclassName ::gMetaClass;                                        \
	/* Class member functions */                                              \
    className :: className(const OSMetaClass *meta)                           \
	: superclassName (meta) { }                                           \
    className ::~ className() { }                                             \
	/* The ::MetaClass constructor */                                         \
    className ::MetaClass::MetaClass() { init; }

#define OSDefineDefaultStructors(className, superclassName)     \
    OSObject * className ::MetaClass::alloc() const { return new className; } \
    className :: className () : superclassName (&gMetaClass) { }

#define OSDefineMetaClassAndStructorsWithInit(className, superclassName, init) \
    OSDefineMetaClassWithInit(className, superclassName, init)        \
    OSDefineDefaultStructors(className, superclassName)

#define OSDefineMetaClassAndStructors(className, superclassName)    \
    OSDefineMetaClassAndStructorsWithInit(className, superclassName, )


class OSMetaClassBase
{
public:
	virtual ~OSMetaClassBase();

	// Add a placeholder we can find later
	virtual void placeholder();

	// Virtual Padding
#ifdef METACLASS_BASE_USED
	OSMetaClassDeclareReservedUsed(OSMetaClassBase, 4);
    virtual int metaclassBaseUsed4();
	OSMetaClassDeclareReservedUsed(OSMetaClassBase, 5);
    virtual int metaclassBaseUsed5();
	OSMetaClassDeclareReservedUsed(OSMetaClassBase, 6);
    virtual int metaclassBaseUsed6();
	OSMetaClassDeclareReservedUsed(OSMetaClassBase, 7);
    virtual int metaclassBaseUsed7();
#else
	virtual void _RESERVEDOSMetaClassBase4();
	virtual void _RESERVEDOSMetaClassBase5();
	virtual void _RESERVEDOSMetaClassBase6();
	virtual void _RESERVEDOSMetaClassBase7();
#endif
};

class OSMetaClass : public OSMetaClassBase {
public:
	virtual ~OSMetaClass();
	// Virtual Padding functions for MetaClass's
	OSMetaClassDeclareReservedUnused(OSMetaClass, 0);
	OSMetaClassDeclareReservedUnused(OSMetaClass, 1);
	OSMetaClassDeclareReservedUnused(OSMetaClass, 2);
	OSMetaClassDeclareReservedUnused(OSMetaClass, 3);
	OSMetaClassDeclareReservedUnused(OSMetaClass, 4);
	OSMetaClassDeclareReservedUnused(OSMetaClass, 5);
	OSMetaClassDeclareReservedUnused(OSMetaClass, 6);
	OSMetaClassDeclareReservedUnused(OSMetaClass, 7);
};

#endif // METACLASS_H
