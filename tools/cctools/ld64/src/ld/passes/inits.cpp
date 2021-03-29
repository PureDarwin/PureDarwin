/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2017 Apple Inc. All rights reserved.
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

#include "ld.hpp"
#include "inits.h"

namespace ld {
namespace passes {
namespace inits {



class InitOffsetAtom : public ld::Atom {
public:
											InitOffsetAtom(const ld::Atom* atom)
											: ld::Atom(_s_section, ld::Atom::definitionRegular, ld::Atom::combineNever,
														ld::Atom::scopeTranslationUnit, ld::Atom::typeUnclassified,
														symbolTableNotIn, false, false, false, ld::Atom::Alignment(2)),
                                             _fixup1(0, ld::Fixup::k1of2, ld::Fixup::kindSetTargetImageOffset, atom),
                                             _fixup2(0, ld::Fixup::k2of2, ld::Fixup::kindStoreLittleEndian32)
                                            { }


	virtual const ld::File*					file() const					{ return NULL; }
	virtual const char*						name() const					{ return "init-offset"; }
	virtual uint64_t						size() const					{ return 4; }
	virtual uint64_t						objectAddress() const			{ return 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const { }
	virtual ld::Fixup::iterator				fixupsBegin() const				{ return &_fixup1; }
	virtual ld::Fixup::iterator				fixupsEnd()	const				{ return &((ld::Fixup*)&_fixup2)[1]; }

protected:
	mutable ld::Fixup						_fixup1;
	ld::Fixup								_fixup2;

	static ld::Section						_s_section;
};

ld::Section InitOffsetAtom::_s_section("__TEXT", "__init_offsets", ld::Section::typeInitOffsets);



void doPass(Options& opts, ld::Internal& state)
{
	//const bool log = false;

	// only transform initializers in final linked image
	if ( opts.outputKind() == Options::kObjectFile )
		return;

    // only transform if targetting new enough OS
    if ( !opts.makeInitializersIntoOffsets() )
        return;

    // transform -init function
    std::vector<const InitOffsetAtom*> initFunctions;
    if ( (opts.initFunctionName() != NULL) && (state.entryPoint != NULL) ) {
        InitOffsetAtom* initOffsetAtom = new InitOffsetAtom(state.entryPoint);
        initFunctions.push_back(initOffsetAtom);
        state.addAtom(*initOffsetAtom);
        state.entryPoint = NULL;  // this prevents LC_ROUTINES from being generated
    }

	// only needed if there is a __mod_init_funcs section
	for (ld::Internal::FinalSection* sect : state.sections) {
		if ( sect->type() != ld::Section::typeInitializerPointers )
			continue;
        const uint64_t pointerSize = (opts.architecture() & CPU_ARCH_ABI64) ? 8 : 4;
        std::vector<InitOffsetAtom*> orderedAtoms;
        for (const ld::Atom* atom : sect->atoms) {
            for (ld::Fixup::iterator fit = atom->fixupsBegin(), end=atom->fixupsEnd(); fit != end; ++fit) {
                if ( fit->firstInCluster() )
                    orderedAtoms.push_back(NULL);
            }
        }
		for (const ld::Atom* atom : sect->atoms) {
			for (ld::Fixup::iterator fit = atom->fixupsBegin(), end=atom->fixupsEnd(); fit != end; ++fit) {
                const Atom* initFunc = NULL;
                switch ( fit->binding ) {
                    case ld::Fixup::bindingDirectlyBound:
                        initFunc = fit->u.target;
                        break;
                    case ld::Fixup::bindingsIndirectlyBound:
                        initFunc = state.indirectBindingTable[fit->u.bindingIndex];
                        break;
                    default:
                        assert(0 && "fixup binding kind unsupported for initializer section");
                        break;
                }
                unsigned index = fit->offsetInAtom / pointerSize;
                assert(index < orderedAtoms.size());
                orderedAtoms[index] = new InitOffsetAtom(initFunc);
            }
        }
        // relocations in .o files may be in random order, so fixups are in random order,
        // but we need initializers to run in order the pointers were in the __mod_init_func section.
        for (InitOffsetAtom* atom : orderedAtoms) {
            initFunctions.push_back(atom);
            state.addAtom(*atom);
        }
	}
    if ( initFunctions.empty() )
        return;



    // remove any old style __mod_init_funcs sections
	state.sections.erase(std::remove_if(state.sections.begin(), state.sections.end(),
                                        [&](ld::Internal::FinalSection*& sect) {
                                            return (sect->type() == ld::Section::typeInitializerPointers);
                                        }), state.sections.end());
}


} // namespace inits
} // namespace passes 
} // namespace ld 
