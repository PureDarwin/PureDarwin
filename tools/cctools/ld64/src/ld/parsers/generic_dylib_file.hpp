/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2015 Apple Inc. All rights reserved.
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

#ifndef __GENERIC_DYLIB_FILE_H__
#define __GENERIC_DYLIB_FILE_H__

#include "ld.hpp"
#include "Options.h"
#include <unordered_map>
#include <unordered_set>

namespace generic {
namespace dylib {

// forward reference
template <typename A> class File;

//
// An ExportAtom has no content.  It exists so that the linker can track which
// imported symbols came from which dynamic libraries.
//
template <typename A>
class ExportAtom final : public ld::Atom
{
public:
	ExportAtom(const File<A>& f, const char* nm, bool weakDef, bool tlv,
			   typename A::P::uint_t address)
		: ld::Atom(f._importProxySection, ld::Atom::definitionProxy,
				   (weakDef ? ld::Atom::combineByName : ld::Atom::combineNever),
				   ld::Atom::scopeLinkageUnit,
				   (tlv ? ld::Atom::typeTLV : ld::Atom::typeUnclassified),
				   symbolTableNotIn, false, false, false, ld::Atom::Alignment(0)),
		  _file(f),
		  _name(nm),
		  _address(address)
	{}

	// overrides of ld::Atom
	virtual const ld::File*	file() const override final { return &_file; }
	virtual const char*		name() const override final { return _name; }
	virtual uint64_t		size() const override final { return 0; }
	virtual uint64_t		objectAddress() const override final { return _address; }
	virtual void			copyRawContent(uint8_t buffer[]) const override final { }

	virtual void			setScope(Scope) { }

private:
	using pint_t = typename A::P::uint_t;

	virtual					~ExportAtom() {}

	const File<A>&			_file;
	const char*				_name;
	pint_t					_address;
};


//
// An ImportAtom has no content.  It exists so that when linking a main executable flat-namespace
// the imports of all flat dylibs are checked
//
template <typename A>
class ImportAtom final : public ld::Atom
{
public:
	ImportAtom(File<A>& f, std::vector<const char*>& imports);

	// overrides of ld::Atom
	virtual ld::File*				file() const override final { return &_file; }
	virtual const char*				name() const override final { return "import-atom"; }
	virtual uint64_t				size() const override final { return 0; }
	virtual uint64_t				objectAddress() const override final { return 0; }
	virtual ld::Fixup::iterator		fixupsBegin() const	override final { return &_undefs[0]; }
	virtual ld::Fixup::iterator		fixupsEnd()	const override final { return &_undefs[_undefs.size()]; }
	virtual void					copyRawContent(uint8_t buffer[]) const override final { }

	virtual void					setScope(Scope)		{ }

private:
	virtual							~ImportAtom() {}

	File<A>&						_file;
	mutable std::vector<ld::Fixup>	_undefs;
};

template <typename A>
ImportAtom<A>::ImportAtom(File<A>& f, std::vector<const char*>& imports)
	: ld::Atom(f._flatDummySection, ld::Atom::definitionRegular, ld::Atom::combineNever,
			   ld::Atom::scopeTranslationUnit, ld::Atom::typeUnclassified, symbolTableNotIn, false,
			   false, false, ld::Atom::Alignment(0)),
	  _file(f)
{
	for(auto *name : imports)
		_undefs.emplace_back(0, ld::Fixup::k1of1, ld::Fixup::kindNone, false, strdup(name));
}

//
// A generic representation for the dynamic library files we support (Mach-O and text-based stubs).
// Common state and functionality is consolidated in this class.
//
template <typename A>
class File : public ld::dylib::File
{
public:
	File(const char* path, time_t mTime, ld::File::Ordinal ordinal, const ld::VersionSet& platforms,
		 bool allowWeakImports, bool linkingFlatNamespace, bool hoistImplicitPublicDylibs,
		 bool allowSimToMacOSX, bool addVers);

	// overrides of ld::File
	virtual bool							forEachAtom(ld::File::AtomHandler&) const override final;
	virtual bool							justInTimeforEachAtom(const char* name, ld::File::AtomHandler&) const override final;
	virtual uint8_t							swiftVersion() const override final { return _swiftVersion; }
	virtual ld::Bitcode*					getBitcode() const override final { return _bitcode.get(); }


	// overrides of ld::dylib::File
	virtual void							processIndirectLibraries(ld::dylib::File::DylibHandler*, bool addImplicitDylibs) override;
	virtual bool							providedExportAtom() const	override final { return _providedAtom; }
	virtual const char*						parentUmbrella() const override final { return _parentUmbrella; }
	virtual const std::vector<const char*>*	allowableClients() const override final { return _allowableClients.empty() ? nullptr : &_allowableClients; }
    virtual const std::vector<const char*>&	rpaths() const override final { return _rpaths; }
	virtual bool							hasWeakExternals() const override final	{ return _hasWeakExports; }
	virtual bool							deadStrippable() const override final { return _deadStrippable; }
	virtual bool							hasWeakDefinition(const char* name) const override final;
    virtual bool                            hasDefinition(const char* name) const override final;
	virtual bool							hasPublicInstallName() const override final { return _hasPublicInstallName; }
	virtual bool							allSymbolsAreWeakImported() const override final;
	virtual bool							installPathVersionSpecific() const override final { return _installPathOverride; }
	virtual bool							appExtensionSafe() const override final	{ return _appExtensionSafe; };
    virtual void                            forEachExportedSymbol(void (^handler)(const char* symbolName, bool weakDef)) const override;


private:
	using pint_t = typename A::P::uint_t;

	friend class ExportAtom<A>;
	friend class ImportAtom<A>;

	struct CStringHash {
		std::size_t operator()(const char* __s) const {
			unsigned long __h = 0;
			for ( ; *__s; ++__s)
				__h = 5 * __h + *__s;
			return size_t(__h);
		};
	};

protected:
	struct AtomAndWeak { ld::Atom* atom; bool weakDef; bool tlv; pint_t address; };
	struct Dependent {
        const char* path;
        File<A>* dylib;
        bool reExport;

        Dependent(const char* path, bool reExport)
            : path(path), dylib(nullptr), reExport(reExport) {}
    };
	struct ReExportChain { ReExportChain* prev; const File* file; };

private:
	using NameToAtomMap = std::unordered_map<const char*, AtomAndWeak, ld::CStringHash, ld::CStringEquals>;
	using NameSet = std::unordered_set<const char*, CStringHash, ld::CStringEquals>;

	std::pair<bool, bool>		hasWeakDefinitionImpl(const char* name) const;
    bool                        hasDefinitionImpl(const char* name) const;
	bool						containsOrReExports(const char* name, bool& weakDef, bool& tlv, pint_t& addr) const;
	void						assertNoReExportCycles(ReExportChain*) const;

protected:
	bool						isPublicLocation(const char* path) const;

private:
	ld::Section							_importProxySection;
	ld::Section							_flatDummySection;
	mutable bool						_providedAtom;
	bool								_indirectDylibsProcessed;

protected:
	mutable NameToAtomMap				_atoms;
	NameSet								_ignoreExports;
	std::vector<Dependent>				_dependentDylibs;
	ImportAtom<A>*						_importAtom;
	std::vector<const char*>   			_allowableClients;
	std::vector<const char*>   			_rpaths;
	const char*							_parentUmbrella;
	std::unique_ptr<ld::Bitcode>		_bitcode;
	ld::VersionSet                      _platforms;
	uint8_t								_swiftVersion;
	bool								_linkingFlat;
	bool								_noRexports;
	bool								_explictReExportFound;
	bool								_implicitlyLinkPublicDylibs;
	bool								_installPathOverride;
	bool								_hasWeakExports;
	bool								_deadStrippable;
	bool								_hasPublicInstallName;
	bool								_appExtensionSafe;
  const bool              _allowWeakImports;
	const bool							_allowSimToMacOSXLinking;
	const bool							_addVersionLoadCommand;

	static bool							_s_logHashtable;
};

template <typename A>
bool File<A>::_s_logHashtable = false;

template <typename A>
File<A>::File(const char* path, time_t mTime, ld::File::Ordinal ord, const ld::VersionSet& platforms,
			  bool allowWeakImports, bool linkingFlatNamespace,
			  bool hoistImplicitPublicDylibs,
			  bool allowSimToMacOSX, bool addVers)
	: ld::dylib::File(path, mTime, ord),
	  _importProxySection("__TEXT", "__import", ld::Section::typeImportProxies, true),
	  _flatDummySection("__LINKEDIT", "__flat_dummy", ld::Section::typeLinkEdit, true),
	  _providedAtom(false),
	  _indirectDylibsProcessed(false),
	  _importAtom(nullptr),
	  _parentUmbrella(nullptr),
    _platforms(platforms),
	  _swiftVersion(0),
	  _linkingFlat(linkingFlatNamespace),
	  _noRexports(false),
	  _explictReExportFound(false),
	  _implicitlyLinkPublicDylibs(hoistImplicitPublicDylibs),
	  _installPathOverride(false),
	  _hasWeakExports(false),
	  _deadStrippable(false),
	  _hasPublicInstallName(false),
	  _appExtensionSafe(false),
    _allowWeakImports(allowWeakImports),
	  _allowSimToMacOSXLinking(allowSimToMacOSX),
	  _addVersionLoadCommand(addVers)
{
}

template <typename A>
std::pair<bool, bool> File<A>::hasWeakDefinitionImpl(const char* name) const
{
	const auto pos = _atoms.find(name);
	if ( pos != this->_atoms.end() )
		return std::make_pair(true, pos->second.weakDef);

	// look in re-exported libraries.
	for (const auto &dep : _dependentDylibs) {
		if ( dep.reExport ) {
			auto ret = dep.dylib->hasWeakDefinitionImpl(name);
			if ( ret.first )
				return ret;
		}
	}
	return std::make_pair(false, false);
}

template <typename A>
bool File<A>::hasDefinitionImpl(const char* name) const
{
    const auto pos = _atoms.find(name);
    if ( pos != this->_atoms.end() )
        return true;

    // look in re-exported libraries.
    for (const auto &dep : _dependentDylibs) {
        if ( dep.reExport ) {
            if ( dep.dylib->hasDefinitionImpl(name) )
                return true;
        }
    }
    return false;
}

template <typename A>
bool File<A>::hasWeakDefinition(const char* name) const
{
	// If we are supposed to ignore this export, then pretend we don't have it.
	if ( _ignoreExports.count(name) != 0 )
		return false;

	return hasWeakDefinitionImpl(name).second;
}

template <typename A>
bool File<A>::hasDefinition(const char* name) const
{
    // If we are supposed to ignore this export, then pretend we don't have it.
    if ( _ignoreExports.count(name) != 0 )
        return false;

    return hasDefinitionImpl(name);
}

template <typename A>
bool File<A>::containsOrReExports(const char* name, bool& weakDef, bool& tlv, pint_t& addr) const
{
	if ( _ignoreExports.count(name) != 0 )
		return false;

	// check myself
	const auto pos = _atoms.find(name);
	if ( pos != _atoms.end() ) {
		weakDef = pos->second.weakDef;
		tlv = pos->second.tlv;
		addr = pos->second.address;
		return true;
	}

	// check dylibs I re-export
	for (const auto& dep : _dependentDylibs) {
		if ( dep.reExport && !dep.dylib->implicitlyLinked() ) {
			if ( dep.dylib->containsOrReExports(name, weakDef, tlv, addr) )
				return true;
		}
	}
	
	return false;
}

template <typename A>
bool File<A>::forEachAtom(ld::File::AtomHandler& handler) const
{
	handler.doFile(*this);

	// if doing flatnamespace and need all this dylib's imports resolve
	// add atom which references alls undefines in this dylib
	if ( _importAtom != nullptr ) {
		handler.doAtom(*_importAtom);
		return true;
	}
	return false;
}

template <typename A>
bool File<A>::justInTimeforEachAtom(const char* name, ld::File::AtomHandler& handler) const
{
	// If we are supposed to ignore this export, then pretend we don't have it.
	if ( _ignoreExports.count(name) != 0 )
		return false;


	AtomAndWeak bucket;
	if ( containsOrReExports(name, bucket.weakDef, bucket.tlv, bucket.address) ) {
		bucket.atom = new ExportAtom<A>(*this, name, bucket.weakDef, bucket.tlv, bucket.address);
		_atoms[name] = bucket;
		_providedAtom = true;
		if ( _s_logHashtable )
			fprintf(stderr, "getJustInTimeAtomsFor: %s found in %s\n", name, this->path());
		// call handler with new export atom
		handler.doAtom(*bucket.atom);
		return true;
	}

	return false;
}

template <typename A>
void File<A>::forEachExportedSymbol(void (^handler)(const char* symbolName, bool weakDef)) const
{
    for (const auto& entry : _atoms) {
        handler(entry.first, entry.second.weakDef);
    }
}

template <typename A>
void File<A>::assertNoReExportCycles(ReExportChain* prev) const
{
	// recursively check my re-exported dylibs
	ReExportChain chain = { prev, this };
	for (const auto &dep : _dependentDylibs) {
		if ( dep.reExport ) {
			auto* child = dep.dylib;
			// check child is not already in chain
			for (auto* p = prev; p != nullptr; p = p->prev) {
				if ( p->file == child ) {
					throwf("cycle in dylib re-exports with %s and %s", child->path(), this->path());
				}
			}
			if ( dep.dylib != nullptr )
				dep.dylib->assertNoReExportCycles(&chain);
		}
	}
}

template <typename A>
void File<A>::processIndirectLibraries(ld::dylib::File::DylibHandler* handler, bool addImplicitDylibs)
{
	// only do this once
	if ( _indirectDylibsProcessed )
		return;

	const static bool log = false;
	if ( log )
		fprintf(stderr, "processIndirectLibraries(%s)\n", this->installPath());
	if ( _linkingFlat ) {
		for (auto &dep : _dependentDylibs)
			dep.dylib = (File<A>*)handler->findDylib(dep.path, this, false);
	}
	else if ( _noRexports ) {
		// MH_NO_REEXPORTED_DYLIBS bit set, then nothing to do
	}
	else {
		// two-level, might have re-exports
		for (auto &dep : this->_dependentDylibs) {
			if ( dep.reExport ) {
				if ( log )
					fprintf(stderr, "processIndirectLibraries() parent=%s, child=%s\n", this->installPath(), dep.path);
				// a LC_REEXPORT_DYLIB, LC_SUB_UMBRELLA or LC_SUB_LIBRARY says we re-export this child
				dep.dylib = (File<A>*)handler->findDylib(dep.path, this, this->speculativelyLoaded());
				if ( dep.dylib->hasPublicInstallName() ) {
					// promote this child to be automatically added as a direct dependent if this already is
					if ( (this->explicitlyLinked() || this->implicitlyLinked()) && (strcmp(dep.path, dep.dylib->installPath()) == 0) ) {
						if ( log )
							fprintf(stderr, "processIndirectLibraries() implicitly linking %s\n", dep.dylib->installPath());
						dep.dylib->setImplicitlyLinked();
					}
					else if ( dep.dylib->explicitlyLinked() || dep.dylib->implicitlyLinked() ) {
						if ( log )
							fprintf(stderr, "processIndirectLibraries() parent is not directly linked, but child is, so no need to re-export child\n");
					}
					else {
						if ( log )
							fprintf(stderr, "processIndirectLibraries() parent is not directly linked, so parent=%s will re-export child=%s\n", this->installPath(), dep.path);
					}
				}
				else {
					// add all child's symbols to me
					if ( log )
						fprintf(stderr, "processIndirectLibraries() child is not public, so parent=%s will re-export child=%s\n", this->installPath(), dep.path);
				}
			}
			else if ( !_explictReExportFound ) {
				// see if child contains LC_SUB_FRAMEWORK with my name
				dep.dylib = (File<A>*)handler->findDylib(dep.path, this, this->speculativelyLoaded());
				const char* parentUmbrellaName = dep.dylib->parentUmbrella();
				if ( parentUmbrellaName != nullptr ) {
					const char* parentName = this->path();
					const char* lastSlash = strrchr(parentName, '/');
					if ( (lastSlash != nullptr) && (strcmp(&lastSlash[1], parentUmbrellaName) == 0) ) {
						// add all child's symbols to me
						dep.reExport = true;
						if ( log )
							fprintf(stderr, "processIndirectLibraries() umbrella=%s will re-export child=%s\n", this->installPath(), dep.path);
					}
				}
			}
		}
	}

	// check for re-export cycles
	ReExportChain chain = { nullptr, this };
	this->assertNoReExportCycles(&chain);
	
	_indirectDylibsProcessed = true;
}

template <typename A>
bool File<A>::isPublicLocation(const char* path) const
{
	// -no_implicit_dylibs disables this optimization
	if ( ! _implicitlyLinkPublicDylibs )
		return false;

	// /usr/lib is a public location
	if ( (strncmp(path, "/usr/lib/", 9) == 0) && (strchr(&path[9], '/') == nullptr) )
		return true;

	// /System/Library/Frameworks/ is a public location
	if ( strncmp(path, "/System/Library/Frameworks/", 27) == 0 ) {
		const char* frameworkDot = strchr(&path[27], '.');
		// but only top level framework
		// /System/Library/Frameworks/Foo.framework/Versions/A/Foo                 ==> true
		// /System/Library/Frameworks/Foo.framework/Resources/libBar.dylib         ==> false
		// /System/Library/Frameworks/Foo.framework/Frameworks/Bar.framework/Bar   ==> false
		// /System/Library/Frameworks/Foo.framework/Frameworks/Xfoo.framework/XFoo ==> false
		if ( frameworkDot != nullptr ) {
			int frameworkNameLen = frameworkDot - &path[27];
			if ( strncmp(&path[strlen(path)-frameworkNameLen-1], &path[26], frameworkNameLen+1) == 0 )
				return true;
		}
	}
	
	return false;
}

// <rdar://problem/5529626> If only weak_import symbols are used, linker should use LD_LOAD_WEAK_DYLIB
template <typename A>
bool File<A>::allSymbolsAreWeakImported() const
{
	bool foundNonWeakImport = false;
	bool foundWeakImport = false;
	//fprintf(stderr, "%s:\n", this->path());
	for (const auto &it : _atoms) {
		auto* atom = it.second.atom;
		if ( atom != nullptr ) {
			if ( atom->weakImported() )
				foundWeakImport = true;
			else
				foundNonWeakImport = true;
			//fprintf(stderr, "  weak_import=%d, name=%s\n", atom->weakImported(), it->first);
		}
	}

	// don't automatically weak link dylib with no imports
	// so at least one weak import symbol and no non-weak-imported symbols must be found
	return foundWeakImport && !foundNonWeakImport;
}


} // end namespace dylib
} // end namespace generic

#endif // __GENERIC_DYLIB_FILE_H__