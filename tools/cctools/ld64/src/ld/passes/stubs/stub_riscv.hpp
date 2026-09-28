// riscv stubs, always non-lazy through a __got slot that dyld binds via chained fixups
// already in ld::passes::stubs namespace
namespace riscv {


class NonLazyPointerAtom : public ld::Atom {
public:
	NonLazyPointerAtom(ld::passes::stubs::Pass& pass, const ld::Atom& stubTo, bool weakImport, bool is64)
				: ld::Atom(_s_section, ld::Atom::definitionRegular,
							ld::Atom::combineNever, ld::Atom::scopeLinkageUnit, ld::Atom::typeNonLazyPointer,
							symbolTableNotIn, false, false, false, ld::Atom::Alignment(is64 ? 3 : 2)),
				_stubTo(stubTo), _is64(is64),
				_fixup1(0, ld::Fixup::k1of1, is64 ? ld::Fixup::kindStoreTargetAddressLittleEndian64
												  : ld::Fixup::kindStoreTargetAddressLittleEndian32, &stubTo) {
					_fixup1.weakImport = weakImport;
					pass.addAtom(*this);
				}

	virtual const ld::File*					file() const					{ return _stubTo.file(); }
	virtual const char*						name() const					{ return _stubTo.name(); }
	virtual uint64_t						size() const					{ return _is64 ? 8 : 4; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const { }
	virtual void							setScope(Scope)					{ }
	virtual ld::Fixup::iterator				fixupsBegin() const				{ return (ld::Fixup*)&_fixup1; }
	virtual ld::Fixup::iterator				fixupsEnd()	const 				{ return &((ld::Fixup*)&_fixup1)[1]; }

private:
	const ld::Atom&							_stubTo;
	bool									_is64;
	ld::Fixup								_fixup1;

	static ld::Section						_s_section;
};

ld::Section NonLazyPointerAtom::_s_section("__DATA", "__got", ld::Section::typeNonLazyPointer);


class NonLazyStubAtom : public ld::Atom {
public:
				NonLazyStubAtom(ld::passes::stubs::Pass& pass, const ld::Atom& stubTo, bool weakImport, bool is64)
				: ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
							ld::Atom::scopeLinkageUnit, ld::Atom::typeStub,
							symbolTableNotIn, false, false, false, ld::Atom::Alignment(2)),
				_stubTo(stubTo), _is64(is64),
				_nonLazyPointer(pass, stubTo, weakImport, is64),
				_fixup1(0, ld::Fixup::k1of2, ld::Fixup::kindSetTargetAddress, &_nonLazyPointer),
				_fixup2(0, ld::Fixup::k2of2, ld::Fixup::kindStoreRISCVhi20PCRel),
				_fixup3(4, ld::Fixup::k1of2, ld::Fixup::kindSetTargetAddress, &_nonLazyPointer),
				_fixup4(4, ld::Fixup::k2of2, ld::Fixup::kindStoreRISCVlo12PCRel) {
					asprintf((char**)&_name, "%s.stub", _stubTo.name());
					pass.addAtom(*this);
				}

	virtual const ld::File*					file() const					{ return _stubTo.file(); }
	virtual const char*						name() const					{ return _name; }
	virtual uint64_t						size() const					{ return 12; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const {
		// the load's immediate is the distance back to the auipc, which the lo12 fixup expects
		OSWriteLittleInt32(&buffer[0], 0, 0x00000E17);                    // auipc t3, %hi(slot)
		OSWriteLittleInt32(&buffer[4], 0, _is64 ? 0xFFCE3E03 : 0xFFCE2E03); // ld/lw t3, %lo(slot)(t3)
		OSWriteLittleInt32(&buffer[8], 0, 0x000E0067);                    // jr    t3
	}
	virtual void							setScope(Scope)					{ }
	virtual ld::Fixup::iterator				fixupsBegin() const				{ return &_fixup1; }
	virtual ld::Fixup::iterator				fixupsEnd()	const 				{ return &((ld::Fixup*)&_fixup4)[1]; }

private:
	const ld::Atom&							_stubTo;
	const char*								_name;
	bool									_is64;
	NonLazyPointerAtom						_nonLazyPointer;
	mutable ld::Fixup						_fixup1;
	mutable ld::Fixup						_fixup2;
	mutable ld::Fixup						_fixup3;
	mutable ld::Fixup						_fixup4;

	static ld::Section						_s_section;
};

ld::Section NonLazyStubAtom::_s_section("__TEXT", "__stubs", ld::Section::typeStub);


} // namespace riscv
