/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2009-2010 Apple Inc. All rights reserved.
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


// already in ld::passes::stubs namespace
namespace x86 {



class FastBindingPointerAtom : public ld::Atom {
public:
											FastBindingPointerAtom(ld::passes::stubs::Pass& pass)
				: ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
							ld::Atom::scopeLinkageUnit, ld::Atom::typeNonLazyPointer, 
							symbolTableNotIn, false, false, false, ld::Atom::Alignment(2)), 
				_fixup(0, ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressLittleEndian32, 
								pass.internal()->compressedFastBinderProxy)
					{ pass.addAtom(*this); }

	virtual const ld::File*					file() const					{ return NULL; }
	virtual const char*						name() const					{ return "fast binder pointer"; }
	virtual uint64_t						size() const					{ return 4; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const { }
	virtual void							setScope(Scope)					{ }
	virtual ld::Fixup::iterator				fixupsBegin() const				{ return &_fixup; }
	virtual ld::Fixup::iterator				fixupsEnd()	const 				{ return &((ld::Fixup*)&_fixup)[1]; }

private:
	mutable ld::Fixup						_fixup;
	
	static ld::Section						_s_section;
};

ld::Section FastBindingPointerAtom::_s_section("__DATA", "__nl_symbol_ptr", ld::Section::typeNonLazyPointer);


class ImageCachePointerAtom : public ld::Atom {
public:
											ImageCachePointerAtom(ld::passes::stubs::Pass& pass)
				: ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
							ld::Atom::scopeTranslationUnit, ld::Atom::typeUnclassified,
							symbolTableIn, false, false, false, ld::Atom::Alignment(2)) { pass.addAtom(*this); }

	virtual const ld::File*					file() const					{ return NULL; }
	virtual const char*						name() const					{ return "__dyld_private"; }
	virtual uint64_t						size() const					{ return 4; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const { }
	virtual void							setScope(Scope)					{ }

private:
	
	static ld::Section						_s_section;
};

ld::Section ImageCachePointerAtom::_s_section("__DATA", "__data", ld::Section::typeUnclassified);





//
//  The stub-helper-helper is the common code factored out of each helper function.
//  It is in the same section as the stub-helpers.  
//  Similar to the PLT0 entry in ELF. 
//
class StubHelperHelperAtom : public ld::Atom {
public:
											StubHelperHelperAtom(ld::passes::stubs::Pass& pass)
				: ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
							ld::Atom::scopeLinkageUnit, ld::Atom::typeStubHelper, 
							symbolTableNotIn, false, false, false, ld::Atom::Alignment(2)), 
				_fixup1(1,  ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressLittleEndian32, compressedImageCache(pass)),
				_fixup2(7, ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressLittleEndian32, compressedFastBinder(pass)) 
					{ pass.addAtom(*this); }

	virtual ld::File*						file() const					{ return NULL; }
	virtual const char*						name() const					{ return "helper helper"; }
	virtual uint64_t						size() const					{ return 12; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const {
			buffer[0]  = 0x68;		// pushl $dyld_ImageLoaderCache
			buffer[1]  = 0x00;
			buffer[2]  = 0x00;
			buffer[3]  = 0x00;
			buffer[4]  = 0x00;
			buffer[5]  = 0xFF;		// jmp *_fast_lazy_bind
			buffer[6]  = 0x25;
			buffer[7]  = 0x00;
			buffer[8]  = 0x00;
			buffer[9]  = 0x00;
			buffer[10] = 0x00;
			buffer[11] = 0x90;		// nop
	}
	virtual void							setScope(Scope)					{ }
	virtual ld::Fixup::iterator				fixupsBegin() const				{ return &_fixup1; }
	virtual ld::Fixup::iterator				fixupsEnd()	const				{ return &((ld::Fixup*)&_fixup2)[1]; }

private:
	static ld::Atom* compressedImageCache(ld::passes::stubs::Pass& pass) {
		if ( pass.compressedImageCache == NULL ) 
			pass.compressedImageCache = new ImageCachePointerAtom(pass);
		return pass.compressedImageCache;		
	}
	static ld::Atom* compressedFastBinder(ld::passes::stubs::Pass& pass) {
		if ( pass.compressedFastBinderPointer == NULL ) 
			pass.compressedFastBinderPointer = new FastBindingPointerAtom(pass);
		return pass.compressedFastBinderPointer;		
	}
	
	mutable ld::Fixup						_fixup1;
	ld::Fixup								_fixup2;
	
	static ld::Section						_s_section;
};

ld::Section StubHelperHelperAtom::_s_section("__TEXT", "__stub_helper", ld::Section::typeStubHelper);



class StubHelperAtom : public ld::Atom {
public:
											StubHelperAtom(ld::passes::stubs::Pass& pass, const ld::Atom* lazyPointer,
														   const ld::Atom& stubTo, bool stubToResolver)
				: ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
							ld::Atom::scopeLinkageUnit, ld::Atom::typeStubHelper, 
							symbolTableNotIn, false, false, false, ld::Atom::Alignment(1)), 
				_stubTo(stubTo),
				_fixup1(1, ld::Fixup::k1of2, ld::Fixup::kindSetLazyOffset, lazyPointer),
				_fixup2(1, ld::Fixup::k2of2, ld::Fixup::kindStoreLittleEndian32),
				_fixup3(6, ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressX86BranchPCRel32, helperHelper(pass, *this, stubToResolver)) { }
				
	virtual const ld::File*					file() const					{ return _stubTo.file(); }
	virtual const char*						name() const					{ return _stubTo.name(); }
	virtual uint64_t						size() const					{ return 10; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const {
			buffer[0]  = 0x68;		// pushl $lazy-info-offset
			buffer[1]  = 0x00;
			buffer[2]  = 0x00;
			buffer[3]  = 0x00;
			buffer[4]  = 0x00;
			buffer[5]  = 0xE9;		// jmp helperhelper
			buffer[6]  = 0x00;
			buffer[7]  = 0x00;
			buffer[8]  = 0x00;
			buffer[9]  = 0x00;
	}
	virtual void							setScope(Scope)					{ }
	virtual ld::Fixup::iterator				fixupsBegin() const				{ return &_fixup1; }
	virtual ld::Fixup::iterator				fixupsEnd() const				{ return &((ld::Fixup*)&_fixup3)[1]; }

private:
	static ld::Atom* helperHelper(ld::passes::stubs::Pass& pass, StubHelperAtom& stub, bool stubToResolver) {
		// hack for no-lazy bind.  StubHelper is not used by needs to be constructed, so use dummy values
		if ( stubToResolver )
			return &stub;
		if ( pass.compressedHelperHelper == NULL )
			pass.compressedHelperHelper = new StubHelperHelperAtom(pass);
		return pass.compressedHelperHelper;
	}

	const ld::Atom&							_stubTo;
	mutable ld::Fixup						_fixup1;
			ld::Fixup						_fixup2;
			ld::Fixup						_fixup3;
	
	static ld::Section						_s_section;
};

ld::Section						StubHelperAtom::_s_section("__TEXT", "__stub_helper", ld::Section::typeStubHelper);


class ResolverHelperAtom : public ld::Atom {
public:
											ResolverHelperAtom(ld::passes::stubs::Pass& pass, const ld::Atom* lazyPointer,
																							const ld::Atom& stubTo)
				: ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
							ld::Atom::scopeLinkageUnit, ld::Atom::typeStubHelper, 
							symbolTableNotIn, false, false, false, ld::Atom::Alignment(1)), 
				_stubTo(stubTo),
				_fixup1(4,  ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressX86BranchPCRel32, &stubTo),
				_fixup2(9,  ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressLittleEndian32, lazyPointer),
				_fixup3(18, ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressLittleEndian32, lazyPointer) { }
				
	virtual const ld::File*					file() const					{ return _stubTo.file(); }
	virtual const char*						name() const					{ return _stubTo.name(); }
	virtual uint64_t						size() const					{ return 22; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const {
			buffer[ 0]  = 0x50;		// push %eax
			buffer[ 1]  = 0x51;		// push %ecx
			buffer[ 2]  = 0x52;		// push %edx
			buffer[ 3]  = 0xE8;		// call foo
			buffer[ 4]  = 0x00;
			buffer[ 5]  = 0x00;		
			buffer[ 6]  = 0x00;
			buffer[ 7]  = 0x00;
			buffer[ 8]  = 0xA3;		// movl	%eax,foo$lazy_ptr
			buffer[ 9]  = 0x00;
			buffer[10]  = 0x00;	
			buffer[11]  = 0x00;
			buffer[12]  = 0x00;
			buffer[13]  = 0x5A;		// pop %edx
			buffer[14]  = 0x59;		// pop %ecx
			buffer[15]  = 0x58;		// pop %eax
			buffer[16]  = 0xFF;		// jmp *foo$lazy_ptr
			buffer[17]  = 0x25;
			buffer[18]  = 0x00;
			buffer[19]  = 0x00;
			buffer[20]  = 0x00;
			buffer[21]  = 0x00;
	}
	virtual void							setScope(Scope)					{ }
	virtual ld::Fixup::iterator				fixupsBegin() const				{ return &_fixup1; }
	virtual ld::Fixup::iterator				fixupsEnd() const				{ return &((ld::Fixup*)&_fixup3)[1]; }

private:

	const ld::Atom&							_stubTo;
	mutable ld::Fixup						_fixup1;
			ld::Fixup						_fixup2;
			ld::Fixup						_fixup3;
	
	static ld::Section						_s_section;
};

ld::Section						ResolverHelperAtom::_s_section("__TEXT", "__stub_helper", ld::Section::typeStubHelper);



class LazyPointerAtom : public ld::Atom {
public:
											LazyPointerAtom(ld::passes::stubs::Pass& pass, const ld::Atom& stubTo,
														bool stubToGlobalWeakDef, bool stubToResolver, bool weakImport)
				: ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
							ld::Atom::scopeTranslationUnit, ld::Atom::typeLazyPointer, 
							symbolTableNotIn, false, false, false, ld::Atom::Alignment(2)), 
				_stubTo(stubTo),
				_helper(pass, this, stubTo, stubToResolver),
				_resolverHelper(pass, this, stubTo),
				_fixup1(0, ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressLittleEndian32, 
									stubToResolver ? &_resolverHelper : (stubToGlobalWeakDef ?  &stubTo : &_helper)),
				_fixup2(0, ld::Fixup::k1of1, ld::Fixup::kindLazyTarget, &stubTo) { 
						_fixup2.weakImport = weakImport; pass.addAtom(*this); 
						if ( stubToResolver )
							pass.addAtom(_resolverHelper);
						else if ( !stubToGlobalWeakDef ) 
							pass.addAtom(_helper); 
				}

	virtual const ld::File*					file() const					{ return _stubTo.file(); }
	virtual const char*						name() const					{ return _stubTo.name(); }
	virtual uint64_t						size() const					{ return 4; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const { }
	virtual void							setScope(Scope)					{ }
	virtual ld::Fixup::iterator				fixupsBegin() const				{ return &_fixup1; }
	virtual ld::Fixup::iterator				fixupsEnd()	const 				{ return &((ld::Fixup*)&_fixup2)[1]; }

private:
	const ld::Atom&							_stubTo;
	StubHelperAtom							_helper;
	ResolverHelperAtom						_resolverHelper;
	mutable ld::Fixup						_fixup1;
	ld::Fixup								_fixup2;
	
	static ld::Section						_s_section;
};

ld::Section LazyPointerAtom::_s_section("__DATA", "__la_symbol_ptr", ld::Section::typeLazyPointer);


class StubAtom : public ld::Atom {
public:
											StubAtom(ld::passes::stubs::Pass& pass, const ld::Atom& stubTo,
													bool stubToGlobalWeakDef, bool stubToResolver, bool weakImport)
				: ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
							ld::Atom::scopeLinkageUnit, ld::Atom::typeStub, 
							symbolTableNotIn, false, false, false, ld::Atom::Alignment(1)), 
				_stubTo(stubTo), 
				_lazyPointer(pass, stubTo, stubToGlobalWeakDef, stubToResolver, weakImport),
				_fixup(2, ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressLittleEndian32, &_lazyPointer) { pass.addAtom(*this); }

	virtual const ld::File*					file() const					{ return _stubTo.file(); }
	virtual const char*						name() const					{ return _stubTo.name(); }
	virtual uint64_t						size() const					{ return 6; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const {
			buffer[0] = 0xFF;		// jmp *foo$lazy_pointer
			buffer[1] = 0x25;
			buffer[2] = 0x00;
			buffer[3] = 0x00;
			buffer[4] = 0x00;
			buffer[5] = 0x00;
	}
	virtual void							setScope(Scope)					{ }
	virtual ld::Fixup::iterator				fixupsBegin() const				{ return &_fixup; }
	virtual ld::Fixup::iterator				fixupsEnd()	const 				{ return &((ld::Fixup*)&_fixup)[1]; }

private:
	const ld::Atom&							_stubTo;
	LazyPointerAtom							_lazyPointer;
	mutable ld::Fixup						_fixup;
	
	static ld::Section						_s_section;
};

ld::Section StubAtom::_s_section("__TEXT", "__symbol_stub", ld::Section::typeStub);


class NonLazyPointerAtom : public ld::Atom {
public:
	NonLazyPointerAtom(ld::passes::stubs::Pass& pass, const ld::Atom& target,
					   bool stubToResolver, bool weakImport)
				: ld::Atom(_s_section, ld::Atom::definitionRegular, 
							ld::Atom::combineNever, ld::Atom::scopeLinkageUnit, ld::Atom::typeNonLazyPointer, 
							symbolTableNotIn, false, false, false, ld::Atom::Alignment(2)),
				_target(target),
				_resolverHelper(pass, this, target),
				_fixup1(0, ld::Fixup::k1of1, ld::Fixup::kindStoreTargetAddressLittleEndian32, (stubToResolver ? &_resolverHelper : &target)) {
					_fixup1.weakImport = weakImport;
					pass.addAtom(*this);
					if ( stubToResolver )
						pass.addAtom(_resolverHelper);
				}

	virtual const ld::File*					file() const					{ return _target.file(); }
	virtual const char*						name() const					{ return _target.name(); }
	virtual uint64_t						size() const					{ return 4; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const { }
	virtual void							setScope(Scope)					{ }
	virtual ld::Fixup::iterator				fixupsBegin() const				{ return (ld::Fixup*)&_fixup1; }
	virtual ld::Fixup::iterator				fixupsEnd()	const 				{ return &((ld::Fixup*)&_fixup1)[1]; }

private:
	const ld::Atom&							_target;
	ResolverHelperAtom						_resolverHelper;
	ld::Fixup								_fixup1;
	
	static ld::Section						_s_section;
};

ld::Section NonLazyPointerAtom::_s_section("__DATA", "__got", ld::Section::typeNonLazyPointer);


class NonLazyStubAtom : public ld::Atom {
public:
	NonLazyStubAtom(ld::passes::stubs::Pass& pass, const ld::Atom& target,
					bool stubToResolver, bool weakImport)
				: ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
							ld::Atom::scopeLinkageUnit, ld::Atom::typeStub, 
							symbolTableNotIn, false, false, false, ld::Atom::Alignment(1)), 
				_target(target),
				_nonlazyPointer(pass, target, stubToResolver, weakImport),
				_fixup1(8, ld::Fixup::k1of4, ld::Fixup::kindSetTargetAddress, &_nonlazyPointer),
				_fixup2(8, ld::Fixup::k2of4, ld::Fixup::kindSubtractTargetAddress, this),
				_fixup3(8, ld::Fixup::k3of4, ld::Fixup::kindSubtractAddend, 5),
				_fixup4(8, ld::Fixup::k4of4, ld::Fixup::kindStoreLittleEndian32) { pass.addAtom(*this); }

	virtual const ld::File*					file() const					{ return _target.file(); }
	virtual const char*						name() const					{ return _target.name(); }
	virtual uint64_t						size() const					{ return 14; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const {
			buffer[0]  = 0xE8;		// call next instruction (picbase)
			buffer[1]  = 0x00;
			buffer[2]  = 0x00;
			buffer[3]  = 0x00;
			buffer[4]  = 0x00;
			buffer[5]  = 0x58;		// pop eax
			buffer[6]  = 0x8D;		// lea foo$nonlazy_pointer-picbase(eax),eax
			buffer[7]  = 0x80;
			buffer[8]  = 0x00;
			buffer[9]  = 0x00;
			buffer[10] = 0x00;
			buffer[11] = 0x00;
			buffer[12] = 0xFF;		// jmp *(eax)
			buffer[13] = 0x20;
	}
	virtual void							setScope(Scope)					{ }
	virtual ld::Fixup::iterator				fixupsBegin() const				{ return &_fixup1; }
	virtual ld::Fixup::iterator				fixupsEnd()	const 				{ return &((ld::Fixup*)&_fixup4)[1]; }

private:
	const ld::Atom&							_target;
	NonLazyPointerAtom						_nonlazyPointer;
	mutable ld::Fixup						_fixup1;
	ld::Fixup								_fixup2;
	ld::Fixup								_fixup3;
	ld::Fixup								_fixup4;

	static ld::Section						_s_section;
};

ld::Section NonLazyStubAtom::_s_section("__TEXT", "__symbol_stub", ld::Section::typeStub);


} // namespace x86

