/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2010-2011 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <dlfcn.h>
#include <mach/machine.h>

#include <vector>
#include <map>
#include <set>

#include "Architectures.hpp"
#include "MachOFileAbstraction.hpp"

#include "ld.hpp"
#include "objc.h"

namespace ld {
namespace passes {
namespace objc {



struct objc_image_info  {
	uint32_t	version;	// initially 0
	uint32_t	flags;
};

#define OBJC_IMAGE_SUPPORTS_GC						(1<<1)
#define OBJC_IMAGE_REQUIRES_GC						(1<<2)
#define OBJC_IMAGE_OPTIMIZED_BY_DYLD				(1<<3)
#define OBJC_IMAGE_SUPPORTS_COMPACTION				(1<<4)
#define OBJC_IMAGE_IS_SIMULATED						(1<<5)
#define OBJC_IMAGE_HAS_CATEGORY_CLASS_PROPERTIES	(1<<6)



//
// This class is the 8 byte section containing ObjC flags
//
template <typename A>
class ObjCImageInfoAtom : public ld::Atom {
public:
											ObjCImageInfoAtom(bool abi2, bool hasCategoryClassProperties, uint8_t swiftVersion, uint16_t swiftLanguageVersion);

	virtual const ld::File*					file() const					{ return NULL; }
	virtual const char*						name() const					{ return "objc image info"; }
	virtual uint64_t						size() const					{ return sizeof(objc_image_info); }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							setScope(Scope)					{ }
	virtual void							copyRawContent(uint8_t buffer[]) const {
		memcpy(buffer, &_content, sizeof(objc_image_info));
	}

private:	
	objc_image_info							_content;

	static ld::Section						_s_sectionABI1;
	static ld::Section						_s_sectionABI2;
};

template <typename A> ld::Section ObjCImageInfoAtom<A>::_s_sectionABI1("__OBJC", "__image_info", ld::Section::typeUnclassified);
template <typename A> ld::Section ObjCImageInfoAtom<A>::_s_sectionABI2("__DATA", "__objc_imageinfo", ld::Section::typeUnclassified);


template <typename A>
ObjCImageInfoAtom<A>::ObjCImageInfoAtom(bool abi2, bool hasCategoryClassProperties, uint8_t swiftVersion, uint16_t swiftLanguageVersion)
	: ld::Atom(abi2 ? _s_sectionABI2 : _s_sectionABI1, ld::Atom::definitionRegular, ld::Atom::combineNever,
							ld::Atom::scopeLinkageUnit, ld::Atom::typeUnclassified, 
							symbolTableNotIn, false, false, false, ld::Atom::Alignment(2))
{  
	
	uint32_t value = 0;
	if ( hasCategoryClassProperties ) {
		value |= OBJC_IMAGE_HAS_CATEGORY_CLASS_PROPERTIES;
	}

	// provide swift ABI version in final binary for runtime to inspect
	value |= (swiftVersion << 8);

	// provide swift language version in final binary for runtime to inspect
	value |= (swiftLanguageVersion << 16);

	_content.version = 0;
	A::P::E::set32(_content.flags, value);
}



//
// This class is for a new Atom which is an ObjC category name created by merging names from categories
//
template <typename A>
class CategoryNameAtom : public ld::Atom {
public:
											CategoryNameAtom(ld::Internal& state,
															const std::vector<const ld::Atom*>* categories);

	virtual const ld::File*					file() const					{ return _file; }
	virtual const char*						name() const					{ return "objc merged category name"; }
	virtual uint64_t						size() const					{ return _categoryName.size() + 1; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							setScope(Scope)					{ }
	virtual void							copyRawContent(uint8_t buffer[]) const {
		strcpy((char*)buffer, _categoryName.c_str());
	}

private:
	typedef typename A::P::uint_t			pint_t;

	const ld::File*							_file;
	std::string								_categoryName;

	static ld::Section						_s_section;
};

template <typename A>
ld::Section CategoryNameAtom<A>::_s_section("__TEXT", "__objc_classname", ld::Section::typeCString);



//
// This class is for a new Atom which is an ObjC method list created by merging method lists from categories
//
template <typename A>
class MethodListAtom : public ld::Atom {
public:
											MethodListAtom(ld::Internal& state, const ld::Atom* baseMethodList, bool meta, 
															const std::vector<const ld::Atom*>* categories, 
															std::set<const ld::Atom*>& deadAtoms);

	virtual const ld::File*					file() const					{ return _file; }
	virtual const char*						name() const					{ return "objc merged method list"; }
	virtual uint64_t						size() const					{ return _methodCount*3*sizeof(pint_t) + 8; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							setScope(Scope)					{ }
	virtual void							copyRawContent(uint8_t buffer[]) const {
		bzero(buffer, size());
		A::P::E::set32(*((uint32_t*)(&buffer[0])), 3*sizeof(pint_t)); // entry size
		A::P::E::set32(*((uint32_t*)(&buffer[4])), _methodCount);
	}
	virtual ld::Fixup::iterator				fixupsBegin() const	{ return (ld::Fixup*)&_fixups[0]; }
	virtual ld::Fixup::iterator				fixupsEnd()	const	{ return (ld::Fixup*)&_fixups[_fixups.size()]; }

private:	
	typedef typename A::P::uint_t			pint_t;

	const ld::File*							_file;
	unsigned int							_methodCount;
	std::vector<ld::Fixup>					_fixups;
	
	static ld::Section						_s_section;
};

template <typename A> 
ld::Section MethodListAtom<A>::_s_section("__DATA", "__objc_const", ld::Section::typeUnclassified);


//
// This class is for a new Atom which is an ObjC protocol list created by merging protocol lists from categories
//
template <typename A>
class ProtocolListAtom : public ld::Atom {
public:
											ProtocolListAtom(ld::Internal& state, const ld::Atom* baseProtocolList, 
															const std::vector<const ld::Atom*>* categories, 
															std::set<const ld::Atom*>& deadAtoms);

	virtual const ld::File*					file() const					{ return _file; }
	virtual const char*						name() const					{ return "objc merged protocol list"; }
	virtual uint64_t						size() const					{ return (_protocolCount+1)*sizeof(pint_t); }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							setScope(Scope)					{ }
	virtual void							copyRawContent(uint8_t buffer[]) const {
		bzero(buffer, size());
		A::P::setP(*((pint_t*)(buffer)), _protocolCount);
	}
	virtual ld::Fixup::iterator				fixupsBegin() const	{ return (ld::Fixup*)&_fixups[0]; }
	virtual ld::Fixup::iterator				fixupsEnd()	const	{ return (ld::Fixup*)&_fixups[_fixups.size()]; }

private:	
	typedef typename A::P::uint_t			pint_t;

	const ld::File*							_file;
	unsigned int							_protocolCount;
	std::vector<ld::Fixup>					_fixups;
	
	static ld::Section						_s_section;
};

template <typename A> 
ld::Section ProtocolListAtom<A>::_s_section("__DATA", "__objc_const", ld::Section::typeUnclassified);



//
// This class is for a new Atom which is an ObjC property list created by merging property lists from categories
//
template <typename A>
class PropertyListAtom : public ld::Atom {
public:
	enum class PropertyKind { ClassProperties, InstanceProperties };

											PropertyListAtom(ld::Internal& state, const ld::Atom* baseProtocolList, 
													 const std::vector<const ld::Atom*>* categories, 
													 std::set<const ld::Atom*>& deadAtoms, 
													 PropertyKind kind);

	virtual const ld::File*					file() const					{ return _file; }
	virtual const char*						name() const					{ return "objc merged property list"; }
	virtual uint64_t						size() const					{ return _propertyCount*2*sizeof(pint_t) + 8; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							setScope(Scope)					{ }
	virtual void							copyRawContent(uint8_t buffer[]) const {
		bzero(buffer, size());
		A::P::E::set32(((uint32_t*)(buffer))[0], 2*sizeof(pint_t)); // sizeof(objc_property)
		A::P::E::set32(((uint32_t*)(buffer))[1], _propertyCount);
	}
	virtual ld::Fixup::iterator				fixupsBegin() const	{ return (ld::Fixup*)&_fixups[0]; }
	virtual ld::Fixup::iterator				fixupsEnd()	const	{ return (ld::Fixup*)&_fixups[_fixups.size()]; }

private:	
	typedef typename A::P::uint_t			pint_t;

	const ld::File*							_file;
	unsigned int							_propertyCount;
	std::vector<ld::Fixup>					_fixups;
	
	static ld::Section						_s_section;
};

template <typename A> 
ld::Section PropertyListAtom<A>::_s_section("__DATA", "__objc_const", ld::Section::typeUnclassified);





//
// This class is used to create an Atom that replaces an atom from a .o file that holds a class_ro_t or category_t.
// It is needed because there is no way to add Fixups to an existing atom.
//
template <typename A>
class ObjCOverlayAtom : public ld::Atom {
public:
											ObjCOverlayAtom(const ld::Atom* classROAtom);

	// overrides of ld::Atom
	virtual const ld::File*				file() const		{ return _atom->file(); }
	virtual const char*					name() const		{ return _atom->name(); }
	virtual uint64_t					size() const		{ return _atom->size(); }
	virtual uint64_t					objectAddress() const { return _atom->objectAddress(); }
	virtual void						copyRawContent(uint8_t buffer[]) const
															{ _atom->copyRawContent(buffer); }
	virtual const uint8_t*				rawContentPointer() const
															{ return _atom->rawContentPointer(); }
	virtual unsigned long				contentHash(const class ld::IndirectBindingTable& ibt) const
															{ return _atom->contentHash(ibt); }
	virtual bool						canCoalesceWith(const ld::Atom& rhs, const class ld::IndirectBindingTable& ibt) const
															{ return _atom->canCoalesceWith(rhs,ibt); }

	virtual ld::Fixup::iterator			fixupsBegin() const	{ return (ld::Fixup*)&_fixups[0]; }
	virtual ld::Fixup::iterator			fixupsEnd()	const	{ return (ld::Fixup*)&_fixups[_fixups.size()]; }

protected:
	void addFixupAtOffset(uint32_t offset);

private:
	typedef typename A::P::uint_t			pint_t;

	const ld::Atom*							_atom;
	std::vector<ld::Fixup>					_fixups;
};

template <typename A>
class ClassROOverlayAtom : public ObjCOverlayAtom<A> {
public:
										ClassROOverlayAtom(const ld::Atom* contentAtom) : ObjCOverlayAtom<A>(contentAtom) { }

	void								addProtocolListFixup();
	void								addPropertyListFixup();
	void								addMethodListFixup();
};

template <typename A>
class CategoryOverlayAtom : public ObjCOverlayAtom<A> {
public:
										CategoryOverlayAtom(const ld::Atom* contentAtom) : ObjCOverlayAtom<A>(contentAtom) { }

	void								addNameFixup();
	void								addInstanceMethodListFixup();
	void								addClassMethodListFixup();
	void								addProtocolListFixup();
	void								addInstancePropertyListFixup();
	void								addClassPropertyListFixup();
};

template <typename A>
ObjCOverlayAtom<A>::ObjCOverlayAtom(const ld::Atom* classROAtom)
	: ld::Atom(classROAtom->section(), ld::Atom::definitionRegular, ld::Atom::combineNever,
			ld::Atom::scopeLinkageUnit, ld::Atom::typeUnclassified, 
			classROAtom->symbolTableInclusion(), false, false, false, classROAtom->alignment()),
	_atom(classROAtom)
{
	// ensure all attributes are same as original
	this->setAttributesFromAtom(*classROAtom);

	// copy fixups from orginal atom
	for (ld::Fixup::iterator fit=classROAtom->fixupsBegin(); fit != classROAtom->fixupsEnd(); ++fit) {
		ld::Fixup fixup = *fit;
		_fixups.push_back(fixup);
	}
}


//
// Base class for reading and updating existing ObjC atoms from .o files
//
template <typename A>
class ObjCData {
public:
	static const ld::Atom*	getPointerInContent(ld::Internal& state, const ld::Atom* contentAtom, unsigned int offset, bool* hasAddend=NULL);
	static void				setPointerInContent(ld::Internal& state, const ld::Atom* contentAtom, 
												unsigned int offset, const ld::Atom* newAtom);
	typedef typename A::P::uint_t			pint_t;
};

template <typename A>
const ld::Atom* ObjCData<A>::getPointerInContent(ld::Internal& state, const ld::Atom* contentAtom, unsigned int offset, bool* hasAddend)
{
	const ld::Atom* target = NULL;
	if ( hasAddend != NULL )
		*hasAddend = false;
	for (ld::Fixup::iterator fit=contentAtom->fixupsBegin(); fit != contentAtom->fixupsEnd(); ++fit) {
		if ( (fit->offsetInAtom == offset) && (fit->kind != ld::Fixup::kindNoneFollowOn) ) {
			switch ( fit->binding ) {
				case ld::Fixup::bindingsIndirectlyBound:
					target = state.indirectBindingTable[fit->u.bindingIndex];
					break;
				case ld::Fixup::bindingDirectlyBound:
					target =  fit->u.target;
					break;
				case ld::Fixup::bindingNone:
					if ( fit->kind == ld::Fixup::kindAddAddend ) {
						if ( hasAddend != NULL )
							*hasAddend = true;
					}
					break;
                 default:
                    break;   
			}
		}
	}
	return target;
}

template <typename A>
void ObjCData<A>::setPointerInContent(ld::Internal& state, const ld::Atom* contentAtom, 
														unsigned int offset, const ld::Atom* newAtom)
{
	for (ld::Fixup::iterator fit=contentAtom->fixupsBegin(); fit != contentAtom->fixupsEnd(); ++fit) {
		if ( fit->offsetInAtom == offset ) {
			switch ( fit->binding ) {
				case ld::Fixup::bindingsIndirectlyBound:
					state.indirectBindingTable[fit->u.bindingIndex] = newAtom;
					return;
				case ld::Fixup::bindingDirectlyBound:
					fit->u.target = newAtom;
					return;
                default:
                     break;    
			}
		}
	}
	assert(0 && "could not update method list");
}



#define GET_FIELD(state, classAtom, field) \
	ObjCData<A>::getPointerInContent(state, classAtom, offsetof(Content, field))
#define SET_FIELD(state, classAtom, field, valueAtom) \
	ObjCData<A>::setPointerInContent(state, classAtom, offsetof(Content, field), valueAtom)

#define GET_RO_FIELD(state, classAtom, field) \
	ObjCData<A>::getPointerInContent(state, getROData(state, classAtom), offsetof(ROContent, field))
#define SET_RO_FIELD(state, classROAtom, field, valueAtom) \
	ObjCData<A>::setPointerInContent(state, getROData(state, classAtom), offsetof(ROContent, field), valueAtom)

//
// Helper class for reading and updating existing ObjC category atoms from .o files
//
template <typename A>
class Category : public ObjCData<A> {
public:
	// Getters
	static const ld::Atom*	getName(ld::Internal& state, const ld::Atom* contentAtom);
	static const ld::Atom*	getClass(ld::Internal& state, const ld::Atom* contentAtom, bool& hasAddend);
	static const ld::Atom*	getInstanceMethods(ld::Internal& state, const ld::Atom* contentAtom);
	static const ld::Atom*	getClassMethods(ld::Internal& state, const ld::Atom* contentAtom);
	static const ld::Atom*	getProtocols(ld::Internal& state, const ld::Atom* contentAtom);
	static const ld::Atom*	getInstanceProperties(ld::Internal& state, const ld::Atom* contentAtom);
	static const ld::Atom*	getClassProperties(ld::Internal& state, const ld::Atom* contentAtom);
	// Setters
	static const ld::Atom*	setName(ld::Internal& state, const ld::Atom* categoryAtom,
									const ld::Atom* categoryNameAtom, std::set<const ld::Atom*>& deadAtoms);
	static const ld::Atom*	setInstanceMethods(ld::Internal& state, const ld::Atom* categoryAtom,
											   const ld::Atom* methodListAtom, std::set<const ld::Atom*>& deadAtoms);
	static const ld::Atom*	setClassMethods(ld::Internal& state, const ld::Atom* categoryAtom,
											const ld::Atom* methodListAtom, std::set<const ld::Atom*>& deadAtoms);
	static const ld::Atom*	setProtocols(ld::Internal& state, const ld::Atom* categoryAtom,
										 const ld::Atom* protocolListAtom, std::set<const ld::Atom*>& deadAtoms);
	static const ld::Atom*	setInstanceProperties(ld::Internal& state, const ld::Atom* categoryAtom,
												  const ld::Atom* propertyListAtom, std::set<const ld::Atom*>& deadAtoms);
	static const ld::Atom*	setClassProperties(ld::Internal& state, const ld::Atom* categoryAtom,
											   const ld::Atom* propertyListAtom, std::set<const ld::Atom*>& deadAtoms);
	static uint32_t         size() { return 6*sizeof(pint_t); }

	static bool				hasCategoryClassPropertiesField(const ld::Atom* categoryAtom);

private:	
	typedef typename A::P::uint_t			pint_t;

	friend class CategoryOverlayAtom<A>;

	struct Content {
		pint_t name;
		pint_t cls;
		pint_t instanceMethods;
		pint_t classMethods;
		pint_t protocols;
		pint_t instanceProperties;
		// Fields below this point are not always present on disk.
		pint_t classProperties;
	};
};


template <typename A>
const ld::Atom*	Category<A>::getName(ld::Internal& state, const ld::Atom* contentAtom)
{
	return GET_FIELD(state, contentAtom, name);
}

template <typename A>
const ld::Atom*	Category<A>::getClass(ld::Internal& state, const ld::Atom* contentAtom, bool& hasAddend)
{
	return ObjCData<A>::getPointerInContent(state, contentAtom, offsetof(Content, cls), &hasAddend); // category_t.cls
}

template <typename A>
const ld::Atom*	Category<A>::getInstanceMethods(ld::Internal& state, const ld::Atom* contentAtom)
{
	return GET_FIELD(state, contentAtom, instanceMethods);
}

template <typename A>
const ld::Atom*	Category<A>::getClassMethods(ld::Internal& state, const ld::Atom* contentAtom)
{
	return GET_FIELD(state, contentAtom, classMethods);
}

template <typename A>
const ld::Atom*	Category<A>::getProtocols(ld::Internal& state, const ld::Atom* contentAtom)
{
	return GET_FIELD(state, contentAtom, protocols);
}

template <typename A>
const ld::Atom*	Category<A>::getInstanceProperties(ld::Internal& state, const ld::Atom* contentAtom)
{
	return GET_FIELD(state, contentAtom, instanceProperties);
}

template <typename A>
const ld::Atom*	Category<A>::getClassProperties(ld::Internal& state, const ld::Atom* contentAtom)
{
	// Only specially-marked files have this field.
	if ( hasCategoryClassPropertiesField(contentAtom) )
		return GET_FIELD(state, contentAtom, classProperties);
	return NULL;
}

template <typename A>
bool Category<A>::hasCategoryClassPropertiesField(const ld::Atom* contentAtom)
{
	// Only specially-marked files have this field.
	if ( const ld::relocatable::File* objFile = dynamic_cast<const ld::relocatable::File*>(contentAtom->file()) ) {
		return objFile->objcHasCategoryClassPropertiesField();
	}
	return false;
}



template <typename A>
const ld::Atom* Category<A>::setName(ld::Internal &state, const ld::Atom *categoryAtom,
									 const ld::Atom *categoryNameAtom, std::set<const ld::Atom *> &deadAtoms)
{
	// if the base category does not already have a method list, we need to create an overlay
	if ( getName(state, categoryAtom) == NULL ) {
		deadAtoms.insert(categoryAtom);
		CategoryOverlayAtom<A>* overlay = new CategoryOverlayAtom<A>(categoryAtom);
		overlay->addNameFixup();
		SET_FIELD(state, overlay, name, categoryNameAtom);
		return overlay;
	}
	SET_FIELD(state, categoryAtom, name, categoryNameAtom);
	return NULL; // means category atom was not replaced
}

template <typename A>
const ld::Atom* Category<A>::setInstanceMethods(ld::Internal& state, const ld::Atom* categoryAtom,
												const ld::Atom* methodListAtom, std::set<const ld::Atom*>& deadAtoms)
{
	// if the base category does not already have a method list, we need to create an overlay
	if ( getInstanceMethods(state, categoryAtom) == NULL ) {
		deadAtoms.insert(categoryAtom);
		CategoryOverlayAtom<A>* overlay = new CategoryOverlayAtom<A>(categoryAtom);
		overlay->addInstanceMethodListFixup();
		SET_FIELD(state, overlay, instanceMethods, methodListAtom);
		return overlay;
	}
	SET_FIELD(state, categoryAtom, instanceMethods, methodListAtom);
	return NULL; // means category atom was not replaced
}

template <typename A>
const ld::Atom* Category<A>::setClassMethods(ld::Internal& state, const ld::Atom* categoryAtom,
											 const ld::Atom* methodListAtom, std::set<const ld::Atom*>& deadAtoms)
{
	// if the base category does not already have a method list, we need to create an overlay
	if ( getClassMethods(state, categoryAtom) == NULL ) {
		deadAtoms.insert(categoryAtom);
		CategoryOverlayAtom<A>* overlay = new CategoryOverlayAtom<A>(categoryAtom);
		overlay->addClassMethodListFixup();
		SET_FIELD(state, overlay, classMethods, methodListAtom);
		return overlay;
	}
	SET_FIELD(state, categoryAtom, classMethods, methodListAtom);
	return NULL; // means category atom was not replaced
}

template <typename A>
const ld::Atom* Category<A>::setProtocols(ld::Internal& state, const ld::Atom* categoryAtom,
										  const ld::Atom* protocolListAtom, std::set<const ld::Atom*>& deadAtoms)
{
	// if the base category does not already have a protocol list, we need to create an overlay
	if ( getProtocols(state, categoryAtom) == NULL ) {
		deadAtoms.insert(categoryAtom);
		CategoryOverlayAtom<A>* overlay = new CategoryOverlayAtom<A>(categoryAtom);
		overlay->addProtocolListFixup();
		SET_FIELD(state, overlay, protocols, protocolListAtom);
		return overlay;
	}
	SET_FIELD(state, categoryAtom, protocols, protocolListAtom);
	return NULL; // means category atom was not replaced
}

template <typename A>
const ld::Atom* Category<A>::setInstanceProperties(ld::Internal& state, const ld::Atom* categoryAtom,
												   const ld::Atom* methodListAtom, std::set<const ld::Atom*>& deadAtoms)
{
	// if the base category does not already have a property list, we need to create an overlay
	if ( getInstanceProperties(state, categoryAtom) == NULL ) {
		deadAtoms.insert(categoryAtom);
		CategoryOverlayAtom<A>* overlay = new CategoryOverlayAtom<A>(categoryAtom);
		overlay->addInstancePropertyListFixup();
		SET_FIELD(state, overlay, instanceProperties, methodListAtom);
		return overlay;
	}
	SET_FIELD(state, categoryAtom, instanceProperties, methodListAtom);
	return NULL; // means category atom was not replaced
}

template <typename A>
const ld::Atom* Category<A>::setClassProperties(ld::Internal& state, const ld::Atom* categoryAtom,
												const ld::Atom* methodListAtom, std::set<const ld::Atom*>& deadAtoms)
{
	// if the base category does not already have a property list, we need to create an overlay
	if ( getClassProperties(state, categoryAtom) == NULL ) {
		deadAtoms.insert(categoryAtom);
		CategoryOverlayAtom<A>* overlay = new CategoryOverlayAtom<A>(categoryAtom);
		overlay->addClassPropertyListFixup();
		SET_FIELD(state, overlay, classProperties, methodListAtom);
		return overlay;
	}
	SET_FIELD(state, categoryAtom, classProperties, methodListAtom);
	return NULL; // means category atom was not replaced
}


template <typename A>
class MethodList : public ObjCData<A> {
public:
	static uint32_t	count(ld::Internal& state, const ld::Atom* methodListAtom) {
		const uint32_t* methodListData = (uint32_t*)(methodListAtom->rawContentPointer());
		return A::P::E::get32(methodListData[1]); // method_list_t.count
	}
};

template <typename A>
class ProtocolList : public ObjCData<A> {
public:
	static uint32_t	count(ld::Internal& state, const ld::Atom* protocolListAtom)  {
		pint_t* protocolListData = (pint_t*)(protocolListAtom->rawContentPointer());
		return A::P::getP(*protocolListData); // protocol_list_t.count
	}
private:
	typedef typename A::P::uint_t	pint_t;
};

template <typename A>
class PropertyList : public ObjCData<A> {
public:
	static uint32_t	count(ld::Internal& state, const ld::Atom* protocolListAtom)  {
		uint32_t* protocolListData = (uint32_t*)(protocolListAtom->rawContentPointer());
		return A::P::E::get32(protocolListData[1]); // property_list_t.count
	}
private:
	typedef typename A::P::uint_t	pint_t;
};



//
// Helper class for reading and updating existing ObjC class atoms from .o files
//
template <typename A>
class Class : public ObjCData<A> {
public:
	static const ld::Atom*	getMetaClass(ld::Internal& state, const ld::Atom* classAtom);
	static const ld::Atom*	getInstanceMethodList(ld::Internal& state, const ld::Atom* classAtom);
	static const ld::Atom*	getInstanceProtocolList(ld::Internal& state, const ld::Atom* classAtom);
	static const ld::Atom*	getInstancePropertyList(ld::Internal& state, const ld::Atom* classAtom);
	static const ld::Atom*	getClassMethodList(ld::Internal& state, const ld::Atom* classAtom);
	static const ld::Atom*	getClassPropertyList(ld::Internal& state, const ld::Atom* classAtom);
	static const ld::Atom*	setInstanceMethodList(ld::Internal& state, const ld::Atom* classAtom, 
												const ld::Atom* methodListAtom, std::set<const ld::Atom*>& deadAtoms);
	static const ld::Atom*	setInstanceProtocolList(ld::Internal& state, const ld::Atom* classAtom, 
												const ld::Atom* protocolListAtom, std::set<const ld::Atom*>& deadAtoms);
	static const ld::Atom*	setInstancePropertyList(ld::Internal& state, const ld::Atom* classAtom, 
												const ld::Atom* propertyListAtom, std::set<const ld::Atom*>& deadAtoms);
	static const ld::Atom*  setClassMethodList(ld::Internal& state, const ld::Atom* classAtom, 
												const ld::Atom* methodListAtom, std::set<const ld::Atom*>& deadAtoms);
	static const ld::Atom*	setClassProtocolList(ld::Internal& state, const ld::Atom* classAtom, 
												const ld::Atom* protocolListAtom, std::set<const ld::Atom*>& deadAtoms);
	static const ld::Atom*	setClassPropertyList(ld::Internal& state, const ld::Atom* classAtom, 
												const ld::Atom* propertyListAtom, std::set<const ld::Atom*>& deadAtoms);
	static uint32_t         size() { return sizeof(Content); }

private:
	friend class ClassROOverlayAtom<A>;

	typedef typename A::P::uint_t			pint_t;

	static const ld::Atom*	getROData(ld::Internal& state, const ld::Atom* classAtom);

	struct Content {
		pint_t isa;
		pint_t superclass;
		pint_t method_cache;
		pint_t vtable;
		pint_t data;
	};

	struct ROContent {
		uint32_t flags;
		uint32_t instanceStart;
		// Note there is 4-bytes of alignment padding between instanceSize 
		// and ivarLayout on 64-bit archs, but no padding on 32-bit archs.
		// This union is a way to model that.
		union {
			uint32_t instanceSize;
			pint_t pad;
		} instanceSize;
		pint_t ivarLayout;
		pint_t name;
		pint_t baseMethods;
		pint_t baseProtocols;
		pint_t ivars;
		pint_t weakIvarLayout;
		pint_t baseProperties;
	};
};

template <typename A>
const ld::Atom*	Class<A>::getMetaClass(ld::Internal& state, const ld::Atom* classAtom)
{
    const ld::Atom* metaClassAtom = GET_FIELD(state, classAtom, isa);
    assert(metaClassAtom != NULL);
    return metaClassAtom;
}

template <typename A>
const ld::Atom*	Class<A>::getROData(ld::Internal& state, const ld::Atom* classAtom)
{
    const ld::Atom* classROAtom = GET_FIELD(state, classAtom, data);
    assert(classROAtom != NULL);
    return classROAtom;
}

template <typename A>
const ld::Atom*	Class<A>::getInstanceMethodList(ld::Internal& state, const ld::Atom* classAtom)
{
	return GET_RO_FIELD(state, classAtom, baseMethods);
}

template <typename A>
const ld::Atom*	Class<A>::getInstanceProtocolList(ld::Internal& state, const ld::Atom* classAtom)
{
	return GET_RO_FIELD(state, classAtom, baseProtocols);
}

template <typename A>
const ld::Atom*	Class<A>::getInstancePropertyList(ld::Internal& state, const ld::Atom* classAtom)
{
	return GET_RO_FIELD(state, classAtom, baseProperties);
}

template <typename A>
const ld::Atom*	Class<A>::getClassMethodList(ld::Internal& state, const ld::Atom* classAtom)
{
	return Class<A>::getInstanceMethodList(state, getMetaClass(state, classAtom));
}

template <typename A>
const ld::Atom*	Class<A>::getClassPropertyList(ld::Internal& state, const ld::Atom* classAtom)
{
    return Class<A>::getInstancePropertyList(state, getMetaClass(state, classAtom));
}

template <typename A>
const ld::Atom* Class<A>::setInstanceMethodList(ld::Internal& state, const ld::Atom* classAtom, 
												const ld::Atom* methodListAtom, std::set<const ld::Atom*>& deadAtoms)
{
	// if the base class does not already have a method list, we need to create an overlay
	if ( getInstanceMethodList(state, classAtom) == NULL ) {
		const ld::Atom* oldROAtom = getROData(state, classAtom);
		deadAtoms.insert(oldROAtom);
		ClassROOverlayAtom<A>* overlay = new ClassROOverlayAtom<A>(oldROAtom);
		//fprintf(stderr, "replace class RO atom %p with %p for method list in class atom %s\n", classROAtom, overlay, classAtom->name());
		overlay->addMethodListFixup();
		SET_FIELD(state, classAtom, data, overlay);
		SET_RO_FIELD(state, classAtom, baseMethods, methodListAtom);
		return overlay;
	}
	SET_RO_FIELD(state, classAtom, baseMethods, methodListAtom);
	return NULL; // means classRO atom was not replaced
}

template <typename A>
const ld::Atom* Class<A>::setInstanceProtocolList(ld::Internal& state, const ld::Atom* classAtom, 
									const ld::Atom* protocolListAtom, std::set<const ld::Atom*>& deadAtoms)
{
	// if the base class does not already have a protocol list, we need to create an overlay
	if ( getInstanceProtocolList(state, classAtom) == NULL ) {
		const ld::Atom* oldROAtom = getROData(state, classAtom);
		deadAtoms.insert(oldROAtom);
		ClassROOverlayAtom<A>* overlay = new ClassROOverlayAtom<A>(oldROAtom);
		//fprintf(stderr, "replace class RO atom %p with %p for protocol list in class atom %s\n", classROAtom, overlay, classAtom->name());
		overlay->addProtocolListFixup();
		SET_FIELD(state, classAtom, data, overlay);
		SET_RO_FIELD(state, classAtom, baseProtocols, protocolListAtom);
		return overlay;
	}
	//fprintf(stderr, "set class RO atom %p protocol list in class atom %s\n", classROAtom, classAtom->name());
	SET_RO_FIELD(state, classAtom, baseProtocols, protocolListAtom);
	return NULL;  // means classRO atom was not replaced
}

template <typename A>
const ld::Atom* Class<A>::setClassProtocolList(ld::Internal& state, const ld::Atom* classAtom, 
									const ld::Atom* protocolListAtom, std::set<const ld::Atom*>& deadAtoms)
{
	// meta class also points to same protocol list as class
	const ld::Atom* metaClassAtom = getMetaClass(state, classAtom);
	//fprintf(stderr, "setClassProtocolList(), classAtom=%p %s, metaClass=%p %s\n", classAtom, classAtom->name(), metaClassAtom, metaClassAtom->name());
	return setInstanceProtocolList(state, metaClassAtom, protocolListAtom, deadAtoms);
}



template <typename A>
const ld::Atom*  Class<A>::setInstancePropertyList(ld::Internal& state, const ld::Atom* classAtom, 
												const ld::Atom* propertyListAtom, std::set<const ld::Atom*>& deadAtoms)
{
	// if the base class does not already have a property list, we need to create an overlay
	if ( getInstancePropertyList(state, classAtom) == NULL ) {
		const ld::Atom* oldROAtom = getROData(state, classAtom);
		deadAtoms.insert(oldROAtom);
		ClassROOverlayAtom<A>* overlay = new ClassROOverlayAtom<A>(oldROAtom);
		//fprintf(stderr, "replace class RO atom %p with %p for property list in class atom %s\n", classROAtom, overlay, classAtom->name());
		overlay->addPropertyListFixup();
		SET_FIELD(state, classAtom, data, overlay);
		SET_RO_FIELD(state, classAtom, baseProperties, propertyListAtom);
		return overlay;
	}
	SET_RO_FIELD(state, classAtom, baseProperties, propertyListAtom);
	return NULL;  // means classRO atom was not replaced
}

template <typename A>
const ld::Atom* Class<A>::setClassMethodList(ld::Internal& state, const ld::Atom* classAtom, 
											const ld::Atom* methodListAtom, std::set<const ld::Atom*>& deadAtoms)
{
	// class methods is just instance methods of metaClass
	return setInstanceMethodList(state, getMetaClass(state, classAtom), methodListAtom, deadAtoms);
}

template <typename A>
const ld::Atom* Class<A>::setClassPropertyList(ld::Internal& state, const ld::Atom* classAtom, 
											const ld::Atom* propertyListAtom, std::set<const ld::Atom*>& deadAtoms)
{
	// class properties is just instance properties of metaClass
	return setInstancePropertyList(state, getMetaClass(state, classAtom), propertyListAtom, deadAtoms);
}

#undef GET_FIELD
#undef SET_FIELD
#undef GET_RO_FIELD
#undef SET_RO_FIELD


template <typename P>
ld::Fixup::Kind pointerFixupKind();

template <> 
ld::Fixup::Kind pointerFixupKind<Pointer32<BigEndian>>() { 
	return ld::Fixup::kindStoreTargetAddressBigEndian32; 
}
template <> 
ld::Fixup::Kind pointerFixupKind<Pointer64<BigEndian>>() { 
	return ld::Fixup::kindStoreTargetAddressBigEndian64; 
}
template <> 
ld::Fixup::Kind pointerFixupKind<Pointer32<LittleEndian>>() { 
	return ld::Fixup::kindStoreTargetAddressLittleEndian32; 
}
template <> 
ld::Fixup::Kind pointerFixupKind<Pointer64<LittleEndian>>() { 
	return ld::Fixup::kindStoreTargetAddressLittleEndian64; 
}

template <typename A>
void ObjCOverlayAtom<A>::addFixupAtOffset(uint32_t offset)
{
	const ld::Atom* targetAtom = this; // temporary
	_fixups.push_back(ld::Fixup(offset, ld::Fixup::k1of1, pointerFixupKind<typename A::P>(), targetAtom));
}


template <typename A>
void ClassROOverlayAtom<A>::addMethodListFixup()
{
	this->addFixupAtOffset(offsetof(typename Class<A>::ROContent, baseMethods));
}

template <typename A>
void ClassROOverlayAtom<A>::addProtocolListFixup()
{
	this->addFixupAtOffset(offsetof(typename Class<A>::ROContent, baseProtocols));
}

template <typename A>
void ClassROOverlayAtom<A>::addPropertyListFixup()
{
	this->addFixupAtOffset(offsetof(typename Class<A>::ROContent, baseProperties));
}

template <typename A>
void CategoryOverlayAtom<A>::addNameFixup()
{
	this->addFixupAtOffset(offsetof(typename Category<A>::Content, name));
}

template <typename A>
void CategoryOverlayAtom<A>::addInstanceMethodListFixup()
{
	this->addFixupAtOffset(offsetof(typename Category<A>::Content, instanceMethods));
}

template <typename A>
void CategoryOverlayAtom<A>::addClassMethodListFixup()
{
	this->addFixupAtOffset(offsetof(typename Category<A>::Content, classMethods));
}

template <typename A>
void CategoryOverlayAtom<A>::addProtocolListFixup()
{
	this->addFixupAtOffset(offsetof(typename Category<A>::Content, protocols));
}

template <typename A>
void CategoryOverlayAtom<A>::addInstancePropertyListFixup()
{
	this->addFixupAtOffset(offsetof(typename Category<A>::Content, instanceProperties));
}

template <typename A>
void CategoryOverlayAtom<A>::addClassPropertyListFixup()
{
	this->addFixupAtOffset(offsetof(typename Category<A>::Content, classProperties));
}




//
// Encapsulates merging of ObjC categories
//
template <typename A>
class OptimizeCategories {
public:
	static void				doit(const Options& opts, ld::Internal& state, bool haveCategoriesWithoutClassPropertyStorage);
	static bool				hasName(ld::Internal& state, const std::vector<const ld::Atom*>* categories);
	static bool				hasInstanceMethods(ld::Internal& state, const std::vector<const ld::Atom*>* categories);
	static bool				hasClassMethods(ld::Internal& state, const std::vector<const ld::Atom*>* categories);
	static bool				hasProtocols(ld::Internal& state, const std::vector<const ld::Atom*>* categories);
	static bool				hasInstanceProperties(ld::Internal& state, const std::vector<const ld::Atom*>* categories);
	static bool				hasClassProperties(ld::Internal& state, const std::vector<const ld::Atom*>* categories);
	
	static unsigned int		class_ro_baseMethods_offset();
private:
	typedef typename A::P::uint_t			pint_t;

};


template <typename A>
bool OptimizeCategories<A>::hasName(ld::Internal& state, const std::vector<const ld::Atom*>* categories)
{
	for (std::vector<const ld::Atom*>::const_iterator it=categories->begin(); it != categories->end(); ++it) {
		const ld::Atom* categoryAtom = *it;
		const ld::Atom* name = Category<A>::getName(state, categoryAtom);
		if ( name != NULL )
			return true;
	}
	return false;
}


template <typename A>
bool OptimizeCategories<A>::hasInstanceMethods(ld::Internal& state, const std::vector<const ld::Atom*>* categories)
{
	for (std::vector<const ld::Atom*>::const_iterator it=categories->begin(); it != categories->end(); ++it) {
		const ld::Atom* categoryAtom = *it;
		const ld::Atom* methodList = Category<A>::getInstanceMethods(state, categoryAtom);
		if ( methodList != NULL ) {
			if ( MethodList<A>::count(state, methodList) > 0 )
				return true;
		}
	}
	return false;
}


template <typename A>
bool OptimizeCategories<A>::hasClassMethods(ld::Internal& state, const std::vector<const ld::Atom*>* categories)
{
	for (std::vector<const ld::Atom*>::const_iterator it=categories->begin(); it != categories->end(); ++it) {
		const ld::Atom* categoryAtom = *it;
		const ld::Atom* methodList = Category<A>::getClassMethods(state, categoryAtom);
		if ( methodList != NULL ) {
			if ( MethodList<A>::count(state, methodList) > 0 )
				return true;
		}
	}
	return false;
}

template <typename A>
bool OptimizeCategories<A>::hasProtocols(ld::Internal& state, const std::vector<const ld::Atom*>* categories)
{
	for (std::vector<const ld::Atom*>::const_iterator it=categories->begin(); it != categories->end(); ++it) {
		const ld::Atom* categoryAtom = *it;
		const ld::Atom* protocolListAtom = Category<A>::getProtocols(state, categoryAtom);
		if ( protocolListAtom != NULL ) {
			if ( ProtocolList<A>::count(state, protocolListAtom) > 0 ) {	
				return true;
			}
		}
	}
	return false;
}


template <typename A>
bool OptimizeCategories<A>::hasInstanceProperties(ld::Internal& state, const std::vector<const ld::Atom*>* categories)
{
	for (std::vector<const ld::Atom*>::const_iterator it=categories->begin(); it != categories->end(); ++it) {
		const ld::Atom* categoryAtom = *it;
		const ld::Atom* propertyListAtom = Category<A>::getInstanceProperties(state, categoryAtom);
		if ( propertyListAtom != NULL ) {
			if ( PropertyList<A>::count(state, propertyListAtom) > 0 )
				return true;
		}
	}
	return false;
}


template <typename A>
bool OptimizeCategories<A>::hasClassProperties(ld::Internal& state, const std::vector<const ld::Atom*>* categories)
{
	for (std::vector<const ld::Atom*>::const_iterator it=categories->begin(); it != categories->end(); ++it) {
		const ld::Atom* categoryAtom = *it;
		const ld::Atom* propertyListAtom = Category<A>::getClassProperties(state, categoryAtom);
		if ( propertyListAtom != NULL ) {
			if ( PropertyList<A>::count(state, propertyListAtom) > 0 )
				return true;
		}
	}
	return false;
}


static const ld::Atom* fixClassAliases(const ld::Atom* classAtom)
{
	if ( (classAtom->size() != 0) || (classAtom->definition() == ld::Atom::definitionProxy) )
		return classAtom;

	for (ld::Fixup::iterator fit=classAtom->fixupsBegin(); fit != classAtom->fixupsEnd(); ++fit) {
		if ( fit->kind == ld::Fixup::kindNoneFollowOn ) {
			assert(fit->offsetInAtom == 0);
			assert(fit->binding == ld::Fixup::bindingDirectlyBound);
			return fit->u.target;
		}
	}

	return classAtom;
}

//
// Helper for std::remove_if
//
class OptimizedAway {
public:
	OptimizedAway(const std::set<const ld::Atom*>& oa) : _dead(oa) {}
	bool operator()(const ld::Atom* atom) const {
		return ( _dead.count(atom) != 0 );
	}
private:
	const std::set<const ld::Atom*>& _dead;
};

	struct AtomSorter
	{	
		bool operator()(const Atom* left, const Atom* right)
		{
			// sort by file ordinal, then object address, then zero size, then symbol name
			// only file based atoms are supported (file() != NULL)
			if (left==right) return false;
			const File *leftf = left->file();
			const File *rightf = right->file();
			
			if (leftf == rightf) {
				if (left->objectAddress() != right->objectAddress()) {
					return left->objectAddress() < right->objectAddress();
				} else {
					// for atoms in the same file with the same address, zero sized
					// atoms must sort before nonzero sized atoms
					if ((left->size() == 0 && right->size() > 0) || (left->size() > 0 && right->size() == 0))
						return left->size() < right->size();
					return strcmp(left->name(), right->name());
				}
			}
			// <rdar://problem/51479025> don't crash if objc atom does not have an owning file, just sort those to end
			if ( leftf == nullptr )
				return false;
			if ( rightf == nullptr )
				return true;
			return  (leftf->ordinal() < rightf->ordinal());
		}
	};
	
	static void sortAtomVector(std::vector<const Atom*> &atoms) {
		std::sort(atoms.begin(), atoms.end(), AtomSorter());
	}


template <typename A>
void OptimizeCategories<A>::doit(const Options& opts, ld::Internal& state, bool haveCategoriesWithoutClassPropertyStorage)
{
	// first find all categories referenced by __objc_nlcatlist section
	std::set<const ld::Atom*> nlcatListAtoms;
	for (std::vector<ld::Internal::FinalSection*>::iterator sit=state.sections.begin(); sit != state.sections.end(); ++sit) {
		ld::Internal::FinalSection* sect = *sit;
		if ( (strcmp(sect->sectionName(), "__objc_nlcatlist") == 0) && (strncmp(sect->segmentName(), "__DATA", 6) == 0) ) {
			for (std::vector<const ld::Atom*>::iterator ait=sect->atoms.begin(); ait != sect->atoms.end(); ++ait) {
				const ld::Atom* categoryListElementAtom = *ait;
				for (unsigned int offset=0; offset < categoryListElementAtom->size(); offset += sizeof(pint_t)) {
					const ld::Atom* categoryAtom = ObjCData<A>::getPointerInContent(state, categoryListElementAtom, offset);
					//fprintf(stderr, "offset=%d, cat=%p %s\n", offset, categoryAtom, categoryAtom->name());
					assert(categoryAtom != NULL);
					nlcatListAtoms.insert(categoryAtom);
				}
			}
		}
	}
	
	// build map of all classes in this image that have categories on them
	typedef std::map<const ld::Atom*, std::vector<const ld::Atom*>*> CatMap;
	CatMap classToCategories;
	std::vector<const ld::Atom*> classOrder;
	std::set<const ld::Atom*> deadAtoms;
	ld::Internal::FinalSection* methodListSection = NULL;
	typedef std::map<const ld::Atom*, std::pair<const ld::Atom*, std::vector<const ld::Atom*>*>> ExternalCatMap;
	ExternalCatMap externalClassToCategories;
	std::vector<const ld::Atom*> externalClassOrder;
	for (std::vector<ld::Internal::FinalSection*>::iterator sit=state.sections.begin(); sit != state.sections.end(); ++sit) {
		ld::Internal::FinalSection* sect = *sit;
		if ( sect->type() == ld::Section::typeObjC2CategoryList ) {
			for (std::vector<const ld::Atom*>::iterator ait=sect->atoms.begin(); ait != sect->atoms.end(); ++ait) {
				const ld::Atom* categoryListElementAtom = *ait;
				bool hasAddend;
				const ld::Atom* categoryAtom = ObjCData<A>::getPointerInContent(state, categoryListElementAtom, 0, &hasAddend);
				if ( hasAddend || (categoryAtom->symbolTableInclusion() ==  ld::Atom::symbolTableNotIn)) {
					//<rdar://problem/8309530> gcc-4.0 uses 'L' labels on categories which disables this optimization
					//warning("__objc_catlist element does not point to start of category");
					continue;
				}
				assert(categoryAtom != NULL);
				assert(categoryAtom->size() >= Category<A>::size());
				// ignore categories also in __objc_nlcatlist
				if ( nlcatListAtoms.count(categoryAtom) != 0 )
					continue;
				const ld::Atom* categoryOnClassAtom = fixClassAliases(Category<A>::getClass(state, categoryAtom, hasAddend));
				assert(categoryOnClassAtom != NULL);

				// <rdar://problem/16107696> for now, back off optimization on new style classes
				if ( hasAddend != 0 )
					continue;
				// <rdar://problem/17249777> don't apply categories to swift classes
				if ( categoryOnClassAtom->hasFixupsOfKind(ld::Fixup::kindNoneGroupSubordinate) )
					continue;

				// only look at classes defined in this image
				if ( categoryOnClassAtom->definition() != ld::Atom::definitionProxy ) {
					CatMap::iterator pos = classToCategories.find(categoryOnClassAtom);
					if ( pos == classToCategories.end() ) {
						classToCategories[categoryOnClassAtom] = new std::vector<const ld::Atom*>();
						classOrder.push_back(categoryOnClassAtom);
					}
					classToCategories[categoryOnClassAtom]->push_back(categoryAtom);
					// mark category atom and catlist atom as dead
					deadAtoms.insert(categoryAtom);
					deadAtoms.insert(categoryListElementAtom);
				} else if ( !haveCategoriesWithoutClassPropertyStorage ) {
					// Note, we only merge duplicate categories which handle the class properties field.  Otherwise we may want to add
					// the class properties from a .o which has it to a category which doesn't
					ExternalCatMap::iterator pos = externalClassToCategories.find(categoryOnClassAtom);
					if ( pos == externalClassToCategories.end() ) {
						externalClassToCategories[categoryOnClassAtom] = { categoryListElementAtom, new std::vector<const ld::Atom*>() };
						externalClassOrder.push_back(categoryOnClassAtom);
					} else {
						// mark category atom and catlist atom as dead as this is not the first category for this class
						deadAtoms.insert(categoryAtom);
						deadAtoms.insert(categoryListElementAtom);
					}
					externalClassToCategories[categoryOnClassAtom].second->push_back(categoryAtom);
				}
			}
		}
		// record method list section
		if ( (strcmp(sect->sectionName(), "__objc_const") == 0) && (strncmp(sect->segmentName(), "__DATA", 6) == 0) )
			methodListSection = sect;
	}

	// Malformed binaries may not have methods lists in __objc_const.  In that case just give up
	if ( methodListSection == NULL )
		return;

	// if found some categories on classes defined in this image
	if ( classToCategories.size() != 0 ) {
		sortAtomVector(classOrder);
		// alter each class definition to have new method list which includes all category methods
		for (std::vector<const ld::Atom*>::iterator it = classOrder.begin(); it != classOrder.end(); it++) {
			const ld::Atom* classAtom = *it;
			const std::vector<const ld::Atom*>* categories = classToCategories[classAtom];
			assert(categories->size() != 0);
			// if any category adds instance methods, generate new merged method list, and replace
			if ( OptimizeCategories<A>::hasInstanceMethods(state, categories) ) { 
				const ld::Atom* baseInstanceMethodListAtom = Class<A>::getInstanceMethodList(state, classAtom); 
				const ld::Atom* newInstanceMethodListAtom = new MethodListAtom<A>(state, baseInstanceMethodListAtom, false, categories, deadAtoms);
				const ld::Atom* newClassRO = Class<A>::setInstanceMethodList(state, classAtom, newInstanceMethodListAtom, deadAtoms);
				// add new method list to final sections
				methodListSection->atoms.push_back(newInstanceMethodListAtom);
				state.atomToSection[newInstanceMethodListAtom] = methodListSection;
				if ( newClassRO != NULL ) {
					assert(strcmp(newClassRO->section().sectionName(), "__objc_const") == 0);
					methodListSection->atoms.push_back(newClassRO);
					state.atomToSection[newClassRO] = methodListSection;
				}
			}
			// if any category adds class methods, generate new merged method list, and replace
			if ( OptimizeCategories<A>::hasClassMethods(state, categories) ) { 
				const ld::Atom* baseClassMethodListAtom = Class<A>::getClassMethodList(state, classAtom); 
				const ld::Atom* newClassMethodListAtom = new MethodListAtom<A>(state, baseClassMethodListAtom, true, categories, deadAtoms);
				const ld::Atom* newClassRO = Class<A>::setClassMethodList(state, classAtom, newClassMethodListAtom, deadAtoms);
				// add new method list to final sections
				methodListSection->atoms.push_back(newClassMethodListAtom);
				state.atomToSection[newClassMethodListAtom] = methodListSection;
				if ( newClassRO != NULL ) {
					assert(strcmp(newClassRO->section().sectionName(), "__objc_const") == 0);
					methodListSection->atoms.push_back(newClassRO);
					state.atomToSection[newClassRO] = methodListSection;
				}
			}
			// if any category adds protocols, generate new merged protocol list, and replace
			if ( OptimizeCategories<A>::hasProtocols(state, categories) ) { 
				const ld::Atom* baseProtocolListAtom = Class<A>::getInstanceProtocolList(state, classAtom); 
				const ld::Atom* newProtocolListAtom = new ProtocolListAtom<A>(state, baseProtocolListAtom, categories, deadAtoms);
				const ld::Atom* newClassRO = Class<A>::setInstanceProtocolList(state, classAtom, newProtocolListAtom, deadAtoms);
				const ld::Atom* newMetaClassRO = Class<A>::setClassProtocolList(state, classAtom, newProtocolListAtom, deadAtoms);
				// add new protocol list to final sections
				methodListSection->atoms.push_back(newProtocolListAtom);
				state.atomToSection[newProtocolListAtom] = methodListSection;
				if ( newClassRO != NULL ) {
					assert(strcmp(newClassRO->section().sectionName(), "__objc_const") == 0);
					methodListSection->atoms.push_back(newClassRO);
					state.atomToSection[newClassRO] = methodListSection;
				}
				if ( newMetaClassRO != NULL ) {
					assert(strcmp(newMetaClassRO->section().sectionName(), "__objc_const") == 0);
					methodListSection->atoms.push_back(newMetaClassRO);
					state.atomToSection[newMetaClassRO] = methodListSection;
				}
			}
			// if any category adds instance properties, generate new merged property list, and replace
			if ( OptimizeCategories<A>::hasInstanceProperties(state, categories) ) { 
				const ld::Atom* basePropertyListAtom = Class<A>::getInstancePropertyList(state, classAtom); 
				const ld::Atom* newPropertyListAtom = new PropertyListAtom<A>(state, basePropertyListAtom, categories, deadAtoms, PropertyListAtom<A>::PropertyKind::InstanceProperties);
				const ld::Atom* newClassRO = Class<A>::setInstancePropertyList(state, classAtom, newPropertyListAtom, deadAtoms);
				// add new property list to final sections
				methodListSection->atoms.push_back(newPropertyListAtom);
				state.atomToSection[newPropertyListAtom] = methodListSection;
				if ( newClassRO != NULL ) {
					assert(strcmp(newClassRO->section().sectionName(), "__objc_const") == 0);
					methodListSection->atoms.push_back(newClassRO);
					state.atomToSection[newClassRO] = methodListSection;
				}
			}
			// if any category adds class properties, generate new merged property list, and replace
			if ( OptimizeCategories<A>::hasClassProperties(state, categories) ) { 
				const ld::Atom* basePropertyListAtom = Class<A>::getClassPropertyList(state, classAtom); 
				const ld::Atom* newPropertyListAtom = new PropertyListAtom<A>(state, basePropertyListAtom, categories, deadAtoms, PropertyListAtom<A>::PropertyKind::ClassProperties);
				const ld::Atom* newClassRO = Class<A>::setClassPropertyList(state, classAtom, newPropertyListAtom, deadAtoms);
				// add new property list to final sections
				methodListSection->atoms.push_back(newPropertyListAtom);
				state.atomToSection[newPropertyListAtom] = methodListSection;
				if ( newClassRO != NULL ) {
					assert(strcmp(newClassRO->section().sectionName(), "__objc_const") == 0);
					methodListSection->atoms.push_back(newClassRO);
					state.atomToSection[newClassRO] = methodListSection;
				}
			}		 
		}
	}

	// if found some duplicate categories on classes defined in another image
	if ( externalClassToCategories.size() != 0 ) {
		sortAtomVector(externalClassOrder);
		// alter each class definition to have new method list which includes all category methods
		for (std::vector<const ld::Atom*>::iterator it = externalClassOrder.begin(); it != externalClassOrder.end(); it++) {
			const ld::Atom* categoryListElementAtom;
			const std::vector<const ld::Atom*>* categories;
			std::tie(categoryListElementAtom, categories) = externalClassToCategories[*it];
			assert(categories->size() != 0);
			// Skip single categories.  We only optimize duplicates
			if (categories->size() == 1)
				continue;

			const ld::Atom* categoryAtom = categories->front();

			// The category name should reflect all the categories we just merged
			// FIXME: Turn this back on once we know how to merge the category name without trashing the indirect symbol index and therefore
			// all other users of this uniqued string
#if 0
			if ( OptimizeCategories<A>::hasName(state, categories) ) {
				ld::Internal::FinalSection* categoryNameSection = state.atomToSection[Category<A>::getName(state, categoryAtom)];
				const ld::Atom* newCategoryNameAtom = new CategoryNameAtom<A>(state, categories);
				const ld::Atom* newCategory = Category<A>::setName(state, categoryAtom, newCategoryNameAtom, deadAtoms);
				// add new category name to final sections
				categoryNameSection->atoms.push_back(newCategoryNameAtom);
				state.atomToSection[newCategoryNameAtom] = categoryNameSection;
				if ( newCategory != NULL ) {
					assert(strcmp(newCategory->section().sectionName(), "__objc_const") == 0);
					categoryNameSection->atoms.push_back(newCategory);
					state.atomToSection[newCategory] = categoryNameSection;
					categoryAtom = newCategory;
					ObjCData<A>::setPointerInContent(state, categoryListElementAtom, 0, newCategory);
				}
			}
#endif

			// if any category adds instance methods, generate new merged method list, and replace
			if ( OptimizeCategories<A>::hasInstanceMethods(state, categories) ) {
				const ld::Atom* newInstanceMethodListAtom = new MethodListAtom<A>(state, nullptr, false, categories, deadAtoms);
				const ld::Atom* newCategory = Category<A>::setInstanceMethods(state, categoryAtom, newInstanceMethodListAtom, deadAtoms);
				// add new method list to final sections
				methodListSection->atoms.push_back(newInstanceMethodListAtom);
				state.atomToSection[newInstanceMethodListAtom] = methodListSection;
				if ( newCategory != NULL ) {
					assert(strcmp(newCategory->section().sectionName(), "__objc_const") == 0);
					methodListSection->atoms.push_back(newCategory);
					state.atomToSection[newCategory] = methodListSection;
					categoryAtom = newCategory;
					ObjCData<A>::setPointerInContent(state, categoryListElementAtom, 0, newCategory);
				}
			}
			// if any category adds class methods, generate new merged method list, and replace
			if ( OptimizeCategories<A>::hasClassMethods(state, categories) ) {
				const ld::Atom* newClassMethodListAtom = new MethodListAtom<A>(state, nullptr, true, categories, deadAtoms);
				const ld::Atom* newCategory = Category<A>::setClassMethods(state, categoryAtom, newClassMethodListAtom, deadAtoms);
				// add new method list to final sections
				methodListSection->atoms.push_back(newClassMethodListAtom);
				state.atomToSection[newClassMethodListAtom] = methodListSection;
				if ( newCategory != NULL ) {
					assert(strcmp(newCategory->section().sectionName(), "__objc_const") == 0);
					methodListSection->atoms.push_back(newCategory);
					state.atomToSection[newCategory] = methodListSection;
					categoryAtom = newCategory;
					ObjCData<A>::setPointerInContent(state, categoryListElementAtom, 0, newCategory);
				}
			}
			// if any category adds protocols, generate new merged protocol list, and replace
			if ( OptimizeCategories<A>::hasProtocols(state, categories) ) {
				const ld::Atom* newProtocolListAtom = new ProtocolListAtom<A>(state, nullptr, categories, deadAtoms);
				const ld::Atom* newCategory = Category<A>::setProtocols(state, categoryAtom, newProtocolListAtom, deadAtoms);
				// add new protocol list to final sections
				methodListSection->atoms.push_back(newProtocolListAtom);
				state.atomToSection[newProtocolListAtom] = methodListSection;
				if ( newCategory != NULL ) {
					assert(strcmp(newCategory->section().sectionName(), "__objc_const") == 0);
					methodListSection->atoms.push_back(newCategory);
					state.atomToSection[newCategory] = methodListSection;
					categoryAtom = newCategory;
					ObjCData<A>::setPointerInContent(state, categoryListElementAtom, 0, newCategory);
				}
			}
			// if any category adds instance properties, generate new merged property list, and replace
			if ( OptimizeCategories<A>::hasInstanceProperties(state, categories) ) {
				const ld::Atom* newPropertyListAtom = new PropertyListAtom<A>(state, nullptr, categories, deadAtoms, PropertyListAtom<A>::PropertyKind::InstanceProperties);
				const ld::Atom* newCategory = Category<A>::setInstanceProperties(state, categoryAtom, newPropertyListAtom, deadAtoms);
				// add new protocol list to final sections
				methodListSection->atoms.push_back(newPropertyListAtom);
				state.atomToSection[newPropertyListAtom] = methodListSection;
				if ( newCategory != NULL ) {
					assert(strcmp(newCategory->section().sectionName(), "__objc_const") == 0);
					methodListSection->atoms.push_back(newCategory);
					state.atomToSection[newCategory] = methodListSection;
					categoryAtom = newCategory;
					ObjCData<A>::setPointerInContent(state, categoryListElementAtom, 0, newCategory);
				}
			}
			// if any category adds class properties, generate new merged property list, and replace
			if ( OptimizeCategories<A>::hasClassProperties(state, categories) ) {
				const ld::Atom* newPropertyListAtom = new PropertyListAtom<A>(state, nullptr, categories, deadAtoms, PropertyListAtom<A>::PropertyKind::ClassProperties);
				const ld::Atom* newCategory = Category<A>::setClassProperties(state, categoryAtom, newPropertyListAtom, deadAtoms);
				// add new protocol list to final sections
				methodListSection->atoms.push_back(newPropertyListAtom);
				state.atomToSection[newPropertyListAtom] = methodListSection;
				if ( newCategory != NULL ) {
					assert(strcmp(newCategory->section().sectionName(), "__objc_const") == 0);
					methodListSection->atoms.push_back(newCategory);
					state.atomToSection[newCategory] = methodListSection;
					categoryAtom = newCategory;
					ObjCData<A>::setPointerInContent(state, categoryListElementAtom, 0, newCategory);
				}
			}
		}
	}

	if ( !classToCategories.empty() || !externalClassToCategories.empty() ) {
		// remove dead atoms
		for (std::vector<ld::Internal::FinalSection*>::iterator sit=state.sections.begin(); sit != state.sections.end(); ++sit) {
			ld::Internal::FinalSection* sect = *sit;
			sect->atoms.erase(std::remove_if(sect->atoms.begin(), sect->atoms.end(), OptimizedAway(deadAtoms)), sect->atoms.end());
		}
	}
}


template <typename A>
CategoryNameAtom<A>::CategoryNameAtom(ld::Internal& state, const std::vector<const ld::Atom*>* categories)
  : ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
			ld::Atom::scopeLinkageUnit, ld::Atom::typeUnclassified,
			symbolTableNotIn, false, false, false, ld::Atom::Alignment(1)), _file(categories->front()->file())
{
	for (const ld::Atom* categoryAtom : *categories) {
		const char* name = (const char*)Category<A>::getName(state, categoryAtom)->rawContentPointer();
		if (!name)
			continue;
		if (_categoryName.empty()) {
			_categoryName = name;
		} else {
			_categoryName = _categoryName + "," + name;
		}
	}
}


template <typename A> 
MethodListAtom<A>::MethodListAtom(ld::Internal& state, const ld::Atom* baseMethodList, bool meta, 
									const std::vector<const ld::Atom*>* categories, std::set<const ld::Atom*>& deadAtoms)
  : ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
			ld::Atom::scopeLinkageUnit, ld::Atom::typeUnclassified, 
			symbolTableNotIn, false, false, false, ld::Atom::Alignment(3)), _file(NULL), _methodCount(0) 
{
	unsigned int fixupCount = 0;
	std::set<const ld::Atom*> baseMethodListMethodNameAtoms;
	// if base class has method list, then associate new method list with file defining class
	if ( baseMethodList != NULL ) {
		_file = baseMethodList->file();
		// calculate total size of merge method lists
		_methodCount = MethodList<A>::count(state, baseMethodList);
		deadAtoms.insert(baseMethodList);
		fixupCount = baseMethodList->fixupsEnd() - baseMethodList->fixupsBegin();
		for (ld::Fixup::iterator fit=baseMethodList->fixupsBegin(); fit != baseMethodList->fixupsEnd(); ++fit) {
			if ( (fit->offsetInAtom - 8) % (3*sizeof(pint_t)) == 0 ) {
				assert(fit->binding == ld::Fixup::bindingsIndirectlyBound && "malformed method list");
				const ld::Atom* target = state.indirectBindingTable[fit->u.bindingIndex];
				assert(target->contentType() == ld::Atom::typeCString && "malformed method list");
				baseMethodListMethodNameAtoms.insert(target);
			}
		}
	}
	for (std::vector<const ld::Atom*>::const_iterator ait=categories->begin(); ait != categories->end(); ++ait) {
		const ld::Atom* categoryMethodListAtom;
		if ( meta )
			categoryMethodListAtom = Category<A>::getClassMethods(state, *ait);
		else
			categoryMethodListAtom = Category<A>::getInstanceMethods(state, *ait);
		if ( categoryMethodListAtom != NULL ) {
			_methodCount += MethodList<A>::count(state, categoryMethodListAtom);
			fixupCount += (categoryMethodListAtom->fixupsEnd() - categoryMethodListAtom->fixupsBegin());
			deadAtoms.insert(categoryMethodListAtom);
			// if base class did not have method list, associate new method list with file the defined category
			if ( _file == NULL )
				_file = categoryMethodListAtom->file();
		}
	}
	//if ( baseMethodList != NULL )
	//	fprintf(stderr, "total merged method count=%u for baseMethodList=%s\n", _methodCount, baseMethodList->name());
	//else
	//	fprintf(stderr, "total merged method count=%u\n", _methodCount);
	//fprintf(stderr, "total merged fixup count=%u\n", fixupCount);
	
	// copy fixups and adjust offsets (in reverse order to simulator objc runtime)
	_fixups.reserve(fixupCount);
	uint32_t slide = 0;
	std::set<const ld::Atom*> categoryMethodNameAtoms;
	for (std::vector<const ld::Atom*>::const_reverse_iterator rit=categories->rbegin(); rit != categories->rend(); ++rit) {
		const ld::Atom* categoryMethodListAtom;
		if ( meta )
			categoryMethodListAtom = Category<A>::getClassMethods(state, *rit);
		else
			categoryMethodListAtom = Category<A>::getInstanceMethods(state, *rit);
		if ( categoryMethodListAtom != NULL ) {
			for (ld::Fixup::iterator fit=categoryMethodListAtom->fixupsBegin(); fit != categoryMethodListAtom->fixupsEnd(); ++fit) {
				ld::Fixup fixup = *fit;
				fixup.offsetInAtom += slide;
				_fixups.push_back(fixup);
				if ( (fixup.offsetInAtom - 8) % (3*sizeof(pint_t)) == 0 ) {
					// <rdar://problem/8642343> warning when a method is overridden in a category in the same link unit
					assert(fixup.binding == ld::Fixup::bindingsIndirectlyBound && "malformed category method list");
					const ld::Atom* target = state.indirectBindingTable[fixup.u.bindingIndex];
					assert(target->contentType() == ld::Atom::typeCString && "malformed method list");
					// this objc pass happens after cstrings are coalesced, so we can just compare the atom addres instead of its content
					if ( baseMethodListMethodNameAtoms.count(target) != 0 ) {
						warning("%s method '%s' in category from %s overrides method from class in %s", 
							(meta ? "meta" : "instance"), target->rawContentPointer(),
							categoryMethodListAtom->safeFilePath(), baseMethodList->safeFilePath() );
					}
					if ( categoryMethodNameAtoms.count(target) != 0 ) {
						warning("%s method '%s' in category from %s conflicts with same method from another category", 
							(meta ? "meta" : "instance"), target->rawContentPointer(),
							categoryMethodListAtom->safeFilePath());
					}
					categoryMethodNameAtoms.insert(target);
				}
			}
			slide += 3*sizeof(pint_t) * MethodList<A>::count(state, categoryMethodListAtom);
		}
	}
	// add method list from base class last
	if ( baseMethodList != NULL ) {
		for (ld::Fixup::iterator fit=baseMethodList->fixupsBegin(); fit != baseMethodList->fixupsEnd(); ++fit) {
			ld::Fixup fixup = *fit;
			fixup.offsetInAtom += slide;
			_fixups.push_back(fixup);
		}
	}
}


template <typename A> 
ProtocolListAtom<A>::ProtocolListAtom(ld::Internal& state, const ld::Atom* baseProtocolList, 
									const std::vector<const ld::Atom*>* categories, std::set<const ld::Atom*>& deadAtoms)
  : ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
			ld::Atom::scopeLinkageUnit, ld::Atom::typeUnclassified, 
			symbolTableNotIn, false, false, false, ld::Atom::Alignment(3)), _file(NULL), _protocolCount(0) 
{
	unsigned int fixupCount = 0;
	if ( baseProtocolList != NULL ) {
		// if base class has protocol list, then associate new protocol list with file defining class
		_file = baseProtocolList->file();
		// calculate total size of merged protocol list
		_protocolCount = ProtocolList<A>::count(state, baseProtocolList);
		deadAtoms.insert(baseProtocolList);
		fixupCount = baseProtocolList->fixupsEnd() - baseProtocolList->fixupsBegin();
	}
	for (std::vector<const ld::Atom*>::const_iterator ait=categories->begin(); ait != categories->end(); ++ait) {
		const ld::Atom* categoryProtocolListAtom = Category<A>::getProtocols(state, *ait);
		if ( categoryProtocolListAtom != NULL ) {
			_protocolCount += ProtocolList<A>::count(state, categoryProtocolListAtom);
			fixupCount += (categoryProtocolListAtom->fixupsEnd() - categoryProtocolListAtom->fixupsBegin());
			deadAtoms.insert(categoryProtocolListAtom);
			// if base class did not have protocol list, associate new protocol list with file the defined category
			if ( _file == NULL )
				_file = categoryProtocolListAtom->file();
		}
	}
	//fprintf(stderr, "total merged protocol count=%u\n", _protocolCount);
	//fprintf(stderr, "total merged fixup count=%u\n", fixupCount);
	
	// copy fixups and adjust offsets 
	_fixups.reserve(fixupCount);
	uint32_t slide = 0;
	for (std::vector<const ld::Atom*>::const_iterator it=categories->begin(); it != categories->end(); ++it) {
		const ld::Atom* categoryProtocolListAtom = Category<A>::getProtocols(state, *it);
		if ( categoryProtocolListAtom != NULL ) {
			for (ld::Fixup::iterator fit=categoryProtocolListAtom->fixupsBegin(); fit != categoryProtocolListAtom->fixupsEnd(); ++fit) {
				ld::Fixup fixup = *fit;
				fixup.offsetInAtom += slide;
				_fixups.push_back(fixup);
				//if ( fixup.binding == ld::Fixup::bindingDirectlyBound )
				//	fprintf(stderr, "offset=0x%08X, name=%s\n", fixup.offsetInAtom, fixup.u.target->name());
			}
			slide += sizeof(pint_t) * ProtocolList<A>::count(state, categoryProtocolListAtom);
		}
	}
	// add method list from base class last
	if ( baseProtocolList != NULL ) {
		for (ld::Fixup::iterator fit=baseProtocolList->fixupsBegin(); fit != baseProtocolList->fixupsEnd(); ++fit) {
			ld::Fixup fixup = *fit;
			fixup.offsetInAtom += slide;
			_fixups.push_back(fixup);
		}
	}
}


template <typename A> 
PropertyListAtom<A>::PropertyListAtom(ld::Internal& state, const ld::Atom* basePropertyList, 
				      const std::vector<const ld::Atom*>* categories, std::set<const ld::Atom*>& deadAtoms, PropertyKind kind)
  : ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
			ld::Atom::scopeLinkageUnit, ld::Atom::typeUnclassified, 
			symbolTableNotIn, false, false, false, ld::Atom::Alignment(3)), _file(NULL), _propertyCount(0) 
{
	unsigned int fixupCount = 0;
	if ( basePropertyList != NULL ) {
		// if base class has property list, then associate new property list with file defining class
		_file = basePropertyList->file();
		// calculate total size of merged property list
		_propertyCount = PropertyList<A>::count(state, basePropertyList);
		deadAtoms.insert(basePropertyList);
		fixupCount = basePropertyList->fixupsEnd() - basePropertyList->fixupsBegin();
	}
	for (std::vector<const ld::Atom*>::const_iterator ait=categories->begin(); ait != categories->end(); ++ait) {
		const ld::Atom* categoryPropertyListAtom = kind == PropertyKind::ClassProperties ? Category<A>::getClassProperties(state, *ait) : Category<A>::getInstanceProperties(state, *ait);
		if ( categoryPropertyListAtom != NULL ) {
			_propertyCount += PropertyList<A>::count(state, categoryPropertyListAtom);
			fixupCount += (categoryPropertyListAtom->fixupsEnd() - categoryPropertyListAtom->fixupsBegin());
			deadAtoms.insert(categoryPropertyListAtom);
			// if base class did not have property list, associate new property list with file the defined category
			if ( _file == NULL )
				_file = categoryPropertyListAtom->file();
		}
	}
	//fprintf(stderr, "total merged property count=%u\n", _propertyCount);
	//fprintf(stderr, "total merged fixup count=%u\n", fixupCount);
	
	// copy fixups and adjust offsets 
	_fixups.reserve(fixupCount);
	uint32_t slide = 0;
	for (std::vector<const ld::Atom*>::const_iterator it=categories->begin(); it != categories->end(); ++it) {
		const ld::Atom* categoryPropertyListAtom = kind == PropertyKind::ClassProperties ? Category<A>::getClassProperties(state, *it) : Category<A>::getInstanceProperties(state, *it);
		if ( categoryPropertyListAtom != NULL ) {
			for (ld::Fixup::iterator fit=categoryPropertyListAtom->fixupsBegin(); fit != categoryPropertyListAtom->fixupsEnd(); ++fit) {
				ld::Fixup fixup = *fit;
				fixup.offsetInAtom += slide;
				_fixups.push_back(fixup);
				//fprintf(stderr, "offset=0x%08X, binding=%d\n", fixup.offsetInAtom, fixup.binding);
				//if ( fixup.binding == ld::Fixup::bindingDirectlyBound )
				//	fprintf(stderr, "offset=0x%08X, name=%s\n", fixup.offsetInAtom, fixup.u.target->name());
				//else if ( fixup.binding == ld::Fixup::bindingsIndirectlyBound )
				//	fprintf(stderr, "offset=0x%08X, indirect index=%u, name=%s\n", fixup.offsetInAtom, fixup.u.bindingIndex, 
				//			(char*)(state.indirectBindingTable[fixup.u.bindingIndex]->rawContentPointer()));
			}
			slide += 2*sizeof(pint_t) * PropertyList<A>::count(state, categoryPropertyListAtom);
		}
	}
	// add method list from base class last
	if ( basePropertyList != NULL ) {
		for (ld::Fixup::iterator fit=basePropertyList->fixupsBegin(); fit != basePropertyList->fixupsEnd(); ++fit) {
			ld::Fixup fixup = *fit;
			fixup.offsetInAtom += slide;
			_fixups.push_back(fixup);
		}
	}
}


template <typename A>
bool scanCategories(ld::Internal& state)
{
	bool warned = false;
	bool haveCategoriesWithoutClassPropertyStorage = false;
	for (std::vector<ld::Internal::FinalSection*>::iterator sit=state.sections.begin(); sit != state.sections.end(); ++sit) {
		ld::Internal::FinalSection* sect = *sit;
		if ( sect->type() == ld::Section::typeObjC2CategoryList ) {
			const char* aFileWithCategorysWithNonNullClassProperties = nullptr;
			for (std::vector<const ld::Atom*>::iterator ait=sect->atoms.begin(); ait != sect->atoms.end(); ++ait) {
				const ld::Atom* categoryListElementAtom = *ait;

				bool hasAddend;
				const ld::Atom* categoryAtom = ObjCData<A>::getPointerInContent(state, categoryListElementAtom, 0, &hasAddend);
				
				if (Category<A>::getClassProperties(state, categoryAtom)) {
					aFileWithCategorysWithNonNullClassProperties = categoryAtom->safeFilePath();
				}

				if ( const ld::relocatable::File* objFile = dynamic_cast<const ld::relocatable::File*>(categoryAtom->file()) ) {
					if ( !objFile->objcHasCategoryClassPropertiesField() ) {
						haveCategoriesWithoutClassPropertyStorage = true;
						if ( aFileWithCategorysWithNonNullClassProperties ) {
							// Complain about mismatched category ABI.
							// These can't be combined into a single linkage unit because there is only one size indicator for all categories in the file.
							// If there is a mismatch then we don't set the HasCategoryClassProperties bit in the output file,
							// which has at runtime causes any class property metadata that was present to be ignored.
							if ( !warned ) {
								warning("Incompatible Objective-C category definitions. Some category metadata may be lost. '%s' and '%s built with different compilers",
										aFileWithCategorysWithNonNullClassProperties, categoryAtom->safeFilePath());
								warned = true;
							}
						}
					}
				}
			}
		}
	}
	return haveCategoriesWithoutClassPropertyStorage;
}


template <typename A, bool isObjC2>
void doPass(const Options& opts, ld::Internal& state)
{
	// Do nothing if the output has no ObjC content.
	if ( !state.hasObjC ) {
	 	return;
	}

	// Search for categories that have a non-null class properties field.
	// Search for categories that do not have storage for the class properties field.
	bool haveCategoriesWithoutClassPropertyStorage = scanCategories<A>(state);
	
	if ( opts.objcCategoryMerging() ) {
		// optimize classes defined in this linkage unit by merging in categories also in this linkage unit
		OptimizeCategories<A>::doit(opts, state, haveCategoriesWithoutClassPropertyStorage);
	}

	// add image info atom
	// The HasCategoryClassProperties bit is set as often as possible.
	state.addAtom(*new ObjCImageInfoAtom<A>(isObjC2, !haveCategoriesWithoutClassPropertyStorage, state.swiftVersion, state.swiftLanguageVersion));
}


void doPass(const Options& opts, ld::Internal& state)
{		
	switch ( opts.architecture() ) {
#if SUPPORT_ARCH_x86_64
		case CPU_TYPE_X86_64:
			doPass<x86_64, true>(opts, state);
			break;
#endif
#if SUPPORT_ARCH_i386
		case CPU_TYPE_I386:
			if (opts.objCABIVersion2POverride()) {
				doPass<x86, true>(opts, state);
			} else {
				doPass<x86, false>(opts, state);
			}
			break;
#endif
#if SUPPORT_ARCH_arm_any
		case CPU_TYPE_ARM:
			doPass<arm, true>(opts, state);
			break;
#endif
#if SUPPORT_ARCH_arm64
		case CPU_TYPE_ARM64:
#if SUPPORT_ARCH_arm64e
			if (opts.subArchitecture() == CPU_SUBTYPE_ARM64E) {
				doPass<arm64e, true>(opts, state);
				break;
			}
#endif
			doPass<arm64, true>(opts, state);
			break;
#endif
		default:
			assert(0 && "unknown objc arch");
	}
}


} // namespace objc
} // namespace passes 
} // namespace ld 
