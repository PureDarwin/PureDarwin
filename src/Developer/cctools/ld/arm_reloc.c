/*
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#ifdef SHLIB
#include "shlib.h"
#endif /* SHLIB */
/*
 * This file contains the routines to do relocation for the arm.
 */
#include <stdlib.h>
#if !(defined(KLD) && defined(__STATIC__))
#include <stdio.h>
#include <mach/mach.h>
#else /* defined(KLD) && defined(__STATIC__) */
#include <mach/kern_return.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include <string.h>
#include <stdarg.h>
#include <mach-o/loader.h>
#include <mach-o/reloc.h>
#include <mach-o/arm/reloc.h>
#include <mach-o/nlist.h>
#include "stuff/bool.h"
#include "stuff/bytesex.h"

#include "ld.h"
#include "live_refs.h"
#include "objects.h"
#include "sections.h"
#include "pass1.h"
#include "symbols.h"
#include "pass2.h"
#include "generic_reloc.h"
#include "arm_reloc.h"
#include "indirect_sections.h"
#include "dylibs.h"

#define U_ABS(l) (((long)(l))<0 ? (unsigned long)(-(l)) : (l))

/*
 * arm_reloc() relocates the contents of the specified section for the 
 * relocation entries using the section map from the current object (cur_obj).
 *
 * Or if refs is not NULL it is being called by to get the addresses or
 * merged_symbols from the item being referenced by the relocation entry(s) at
 * reloc_index. This is used by mark_fine_relocs_references_live() when
 * -dead_strip is specified to determined what is being referenced and is only
 * called when all sections have fine_relocs (that is why refs is only filled
 * in when nfine_relocs != 0). When refs is not NULL, only refs is filled in
 * and returned and the contents are not relocated.
 */
__private_extern__
void
arm_reloc(
char *contents,
struct relocation_info *relocs,
struct section_map *section_map,
struct live_refs *refs,
unsigned long reloc_index)
{
    unsigned long i, j, symbolnum, value, input_pc, output_pc;
    unsigned long instruction, immediate, low_bit;
    struct nlist *nlists;
    char *strings;
    enum bool force_extern_reloc, relocated_extern_thumb_symbol;
    enum bool relocated_extern_arm_symbol;
    struct undefined_map *undefined_map;
    struct merged_symbol *merged_symbol;
    struct section_map *local_map, *pair_local_map;
    struct relocation_info *reloc, *pair_reloc;
    struct scattered_relocation_info *sreloc, *spair_reloc;
    unsigned long r_address, r_symbolnum, r_pcrel, r_length, r_extern,
		  r_scattered, r_value, pair_r_symbolnum, pair_r_value;
    enum reloc_type_arm r_type, pair_r_type;
    unsigned long other_half;
    unsigned long offset;
    unsigned long br14_disp_sign;

#if defined(DEBUG) || defined(RLD)
	/*
	 * The compiler "warnings: ... may be used uninitialized in this
	 * function" can safely be ignored
	 */
	merged_symbol = NULL;
	local_map = NULL;
	instruction = 0;
	other_half = 0;
	immediate = 0;
	offset = 0;
	pair_r_symbolnum = 0;
	pair_r_value = 0;
	pair_local_map = NULL;
#endif /* defined(DEBUG) || defined(RLD) */

	if(refs != NULL)
	    memset(refs, '\0', sizeof(struct live_refs));
	else
	    reloc_index = 0;
	for(i = reloc_index; i < section_map->s->nreloc; i++){
	    br14_disp_sign = 0;
	    force_extern_reloc = FALSE;
	    relocated_extern_thumb_symbol = FALSE;
	    relocated_extern_arm_symbol = FALSE;
	    /*
	     * Break out the fields of the relocation entry and set pointer to
	     * the type of relocation entry it is (for updating later).
	     */
	    if((relocs[i].r_address & R_SCATTERED) != 0){
		sreloc = (struct scattered_relocation_info *)(relocs + i);
		reloc = NULL;
		r_scattered = 1;
		r_address = sreloc->r_address;
		r_pcrel = sreloc->r_pcrel;
		r_length = sreloc->r_length;
		r_value = sreloc->r_value;
		r_type = (enum reloc_type_arm)sreloc->r_type;
		r_extern = 0;
		/*
		 * Since the r_value field is reserved in a ARM_RELOC_PAIR
		 * type to report the correct error a check for a stray
		 * ARM_RELOC_PAIR relocation types needs to be done before
		 * it is assumed that r_value is legal.  A ARM_RELOC_PAIR
		 * only follows ARM_RELOC_{SECTDIFF,LOCAL_SECTDIFF} relocation
		 * types and it is an error to see one otherwise.
		 */
		if(r_type == ARM_RELOC_PAIR){
		    error_with_cur_obj("stray relocation ARM_RELOC_PAIR entry "
			"(%lu) in section (%.16s,%.16s)", i,
			section_map->s->segname, section_map->s->sectname);
		    continue;
		}
		/* calculate the r_symbolnum (n_sect) from the r_value */
		r_symbolnum = 0;
		for(j = 0; j < cur_obj->nsection_maps; j++){
		    if(r_value >= cur_obj->section_maps[j].s->addr &&
		       r_value < cur_obj->section_maps[j].s->addr +
				 cur_obj->section_maps[j].s->size){
			r_symbolnum = j + 1;
			break;
		    }
		}
		if(r_symbolnum == 0){
		    /*
		     * The edge case where the last address past then end of
		     * of the last section is referenced.
		     */
		    for(j = 0; j < cur_obj->nsection_maps; j++){
			if(r_value == cur_obj->section_maps[j].s->addr +
				      cur_obj->section_maps[j].s->size){
			    r_symbolnum = j + 1;
			    break;
			}
		    }
		    if(r_symbolnum == 0){
			error_with_cur_obj("r_value (0x%x) field of relocation "
			    "entry %lu in section (%.16s,%.16s) out of range",
			    (unsigned int)r_value, i, section_map->s->segname,
			    section_map->s->sectname);
			return;
		    }
		}
	    }
	    else{
		reloc = relocs + i;
		sreloc = NULL;
		r_scattered = 0;
		r_address = reloc->r_address;
		r_pcrel = reloc->r_pcrel;
		r_length = reloc->r_length;
		r_extern = reloc->r_extern;
		r_symbolnum = reloc->r_symbolnum;
		r_type = (enum reloc_type_arm)reloc->r_type;
		r_value = 0;
	    }
	    /*
	     * ARM_RELOC_PAIR relocation types only follows ARM_RELOC_{SECTDIFF,
	     * LOCAL_SECTDIFF} relocation types and it is an error to see one
	     * otherwise.
	     */
	    if(r_type == ARM_RELOC_PAIR){
		error_with_cur_obj("stray relocation ARM_RELOC_PAIR entry "
		    "(%lu) in section (%.16s,%.16s)", i,
		    section_map->s->segname, section_map->s->sectname);
		continue;
	    }
	    /*
	     * The r_address field is really an offset into the contents of the
	     * section and must reference something inside the section (Note
	     * that this is not the case for ARM_RELOC_PAIR entries but this
	     * can't be one with the above checks).
	     */
	    if(r_address >= section_map->s->size){
		error_with_cur_obj("r_address (0x%x) field of relocation entry "
		    "%lu in section (%.16s,%.16s) out of range",
		    (unsigned int)r_address, i, section_map->s->segname,
		    section_map->s->sectname);
		return;
	    }
	    /*
	     * If this relocation type is to have a pair make sure it is there
	     * and then break out it's fields.
	     */
	    pair_r_type = (enum reloc_type_arm)0;
	    pair_reloc = NULL;
	    spair_reloc = NULL;
	    if(r_type == ARM_RELOC_SECTDIFF ||
	       r_type == ARM_RELOC_LOCAL_SECTDIFF){
		if(r_scattered != 1){
		    error_with_cur_obj("relocation entry (%lu) in section "
			"(%.16s,%.16s) r_type is ARM_RELOC_SECTDIFF but "
			"relocation entry not scattered type", i,
			section_map->s->segname, section_map->s->sectname);
		    continue;
		}
		if(i + 1 < section_map->s->nreloc){
		    pair_reloc = relocs + i + 1;
		    if((pair_reloc->r_address & R_SCATTERED) != 0){
			spair_reloc = (struct scattered_relocation_info *)
				      pair_reloc;
			pair_reloc = NULL;
			pair_r_type = (enum reloc_type_arm)spair_reloc->r_type;
			pair_r_value = spair_reloc->r_value;
			other_half = spair_reloc->r_address;
		    }
		    else{
			error_with_cur_obj("relocation entry (%lu) in section "
			    "(%.16s,%.16s) following associated relocation "
			    "entry not scattered type", i,
			    section_map->s->segname, section_map->s->sectname);
			continue;
		    }
		}
		if((pair_reloc == NULL && spair_reloc == NULL) ||
		   pair_r_type != ARM_RELOC_PAIR){
		    error_with_cur_obj("relocation entry (%lu) in section "
			"(%.16s,%.16s) missing following associated "
			"ARM_RELOC_PAIR entry", i, section_map->s->segname,
			section_map->s->sectname);
		    continue;
		}
		/*
		 * Calculate the pair_r_symbolnum (n_sect) from the
		 * pair_r_value.
		 */
		pair_r_symbolnum = 0;
		for(j = 0; j < cur_obj->nsection_maps; j++){
		    if(pair_r_value >= cur_obj->section_maps[j].s->addr &&
		       pair_r_value < cur_obj->section_maps[j].s->addr +
				 cur_obj->section_maps[j].s->size){
			pair_r_symbolnum = j + 1;
			break;
		    }
		}
		if(pair_r_symbolnum == 0){
		    error_with_cur_obj("r_value (0x%x) field of relocation "
			"entry %lu in section (%.16s,%.16s) out of range",
			(unsigned int)r_value, i + 1, section_map->s->segname,
			section_map->s->sectname);
		    return;
		}
	    }
	    /*
	     * If r_extern is set this relocation entry is an external entry
	     * else it is a local entry (or scattered entry).
	     */
	    if(r_extern){
		/*
		 * This is an external relocation entry.  So the value to be
		 * added to the item to be relocated is the value of the symbol.
		 * r_symbolnum is an index into the input file's symbol table
		 * of the symbol being refered to.  The symbol must be an
		 * undefined or coalesced symbol to be used in an external
		 * relocation entry.
		 */
		if(r_symbolnum >= cur_obj->symtab->nsyms){
		    error_with_cur_obj("r_symbolnum (%lu) field of external "
			"relocation entry %lu in section (%.16s,%.16s) out of "
			"range", r_symbolnum, i, section_map->s->segname,
			section_map->s->sectname);
		    return;
		}
		symbolnum = r_symbolnum;
		undefined_map = bsearch(&symbolnum, cur_obj->undefined_maps,
		    cur_obj->nundefineds, sizeof(struct undefined_map),
		    (int (*)(const void *, const void *))undef_bsearch);
		if(undefined_map != NULL){
		    merged_symbol = undefined_map->merged_symbol;
		}
		else{
		    nlists = (struct nlist *)(cur_obj->obj_addr +
					      cur_obj->symtab->symoff);
		    strings = (char *)(cur_obj->obj_addr +
				       cur_obj->symtab->stroff);
		    if((nlists[symbolnum].n_type & N_EXT) != N_EXT){
			error_with_cur_obj("r_symbolnum (%lu) field of external"
			    " relocation entry %lu in section (%.16s,%.16s) "
			    "refers to a non-external symbol", symbolnum, i,
			    section_map->s->segname, section_map->s->sectname);
			return;
		    }
		    /*
		     * We must allow and create references to defined global
		     * coalesced symbols with external relocation entries so
		     * that the dynamic linker can relocate all references to
		     * the same symbol.
		     */
		    if((nlists[symbolnum].n_type & N_TYPE) == N_SECT &&
		       (cur_obj->section_maps[nlists[symbolnum].n_sect-1].
			s->flags & SECTION_TYPE) == S_COALESCED){
			merged_symbol = lookup_symbol(strings +
					     nlists[symbolnum].n_un.n_strx);
			if(merged_symbol->name_len == 0){
			    fatal("internal error, in arm_reloc() failed to "
			          "lookup coalesced symbol %s", strings +
				  nlists[symbolnum].n_un.n_strx);
			}
		    }
		    else{
			if((nlists[symbolnum].n_type & N_EXT) != N_EXT ||
			   (nlists[symbolnum].n_type & N_TYPE) != N_UNDF){
			    error_with_cur_obj("r_symbolnum (%lu) field of "
				"external relocation entry %lu in section "
				"(%.16s,%.16s) refers to a non-undefined "
				"symbol", symbolnum, i, section_map->s->segname,
				 section_map->s->sectname);
			    return;
			}
			print_obj_name(cur_obj);
			fatal("internal error, in arm_reloc() symbol index %lu "
			    "in above file not in undefined map", symbolnum);
		    }
		}
		if(refs == NULL && 
		   ((merged_symbol->nlist.n_type & N_TYPE) == N_SECT &&
		    (get_output_section(merged_symbol->nlist.n_sect)->
		     flags & SECTION_TYPE) == S_COALESCED)){
		    if(((merged_symbol->nlist.n_type & N_PEXT) == N_PEXT &&
			keep_private_externs == FALSE) ||
		       dynamic == FALSE ||
		       (output_for_dyld && has_dynamic_linker_command))
			force_extern_reloc = FALSE;
		    else
			force_extern_reloc = TRUE;
		}
		/*
		 * If this is an indirect symbol resolve indirection (all chains
		 * of indirect symbols have been resolved so that they point at
		 * a symbol that is not an indirect symbol).
		 */
		if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR)
		    merged_symbol = (struct merged_symbol *)
				    merged_symbol->nlist.n_value;

		/*
		 * If we are being called only to get the references for this
		 * relocation entry fill it in and return.
		 */
		if(refs != NULL){
		    refs->ref1.ref_type = LIVE_REF_SYMBOL;
		    refs->ref1.merged_symbol = merged_symbol;
		    refs->ref2.ref_type = LIVE_REF_NONE;
		    return;
		}

		/*
		 * If the symbol is undefined (or common) or a global coalesced 
		 * symbol where we need to force an external relocation entry
		 * and we are not prebinding no relocation is done.  Or if the
		 * output file is a multi module MH_DYLIB no relocation is done
		 * unless the symbol is a private extern or we are prebinding.
		 */
		if(((merged_symbol->nlist.n_type & N_TYPE) == N_UNDF) ||
		   (force_extern_reloc == TRUE && prebinding == FALSE) ||
		   ((filetype == MH_DYLIB && multi_module_dylib == TRUE) &&
		    (((merged_symbol->nlist.n_type & N_PEXT) != N_PEXT) &&
		     prebinding == FALSE) ) )
		    value = 0;
		else{
		    value = merged_symbol->nlist.n_value;
		    /*
		     * Pointers to thumb symbols must have their low bit set
		     * only after they have been relocated.  Also arm BL 
		     * instructions to thumb symbols must be conveted to BLX
		     * instructions. This external relocation entry will be
		     * relocated, so if it is for a thumb symbol then set
		     * relocated_extern_thumb_symbol so it can be used later.
		     */
		    if((merged_symbol->nlist.n_desc & N_ARM_THUMB_DEF))
			relocated_extern_thumb_symbol = TRUE;
		    else
			relocated_extern_arm_symbol = TRUE;
		    /*
		     * To know which type (local or scattered) of relocation
		     * entry to convert this one to (if relocation entries are
		     * saved) the offset to be added to the symbol's value is
		     * needed to see if it reaches outside the block in which
		     * the symbol is in.  In here if the offset is not zero then
		     * it is assumed to reach out of the block and a scattered
		     * relocation entry is used.
		     */
		    input_pc = section_map->s->addr + r_address;
		    if(r_type == ARM_RELOC_VANILLA){
			switch(r_length){
			case 0: /* byte */
			    offset = get_byte((char *)(contents + r_address));
			    break;
			case 1: /* word (2 byte) */
			    offset = get_short((short *)(contents + r_address));
			    break;
			case 2: /* long (4 byte) */
			    offset = get_long((long *)(contents + r_address));
			    break;
			default:
			    /* the error check is catched below */
			    break;
			}
			if(r_pcrel)
			    offset += input_pc;
		    }
		    else{
			instruction = get_long((long *)(contents + r_address));
			switch(r_type){
			case ARM_RELOC_BR24:
			    offset = instruction & 0x00ffffff;
			    /* sign extend if needed */
			    if((offset & 0x00800000) != 0)
				offset |= 0xff000000;
			    /* The value in the instruction is shifted by 2 */
			    offset = offset << 2;
			    /*
			     * Note the pc added will be +8 from the pc of the
			     * branch instruction.  And the assembler creating
			     * this instruction takes that into account when
			     * calculating the displacement in the instruction.
			     */
			    if(r_pcrel)
				offset += input_pc;
			    break;
			case ARM_THUMB_RELOC_BR22:
			    /*
			     * The code below assumes ARM is little endian
			     * such that "the first 16-bit thumb instruction"
			     * is the low 16 bits and "the second 16-bit thumb	
			     * instruction" is the high 16 bits of the 32-bits
			     * in the variable instruction.
			     */
			    /* the first instruction has the upper eleven bits 
			       of the two byte displacement */
			    offset = (instruction & 0x7FF) << 12;
			    /* sign extend if needed */
			    if((offset & 0x400000) != 0)
				offset |= 0xFF800000;
			    /* the second instruction has the lower eleven bits 
			        of the two byte displacement.  Add that times
				two to get the offset added to the symbol */
			    offset += 2*((instruction >> 16) & 0x7FF);
			    /*
			     * Note the pc added will be +4 from the pc of the
			     * branch instruction.  And the assembler creating
			     * this instruction takes that into account when
			     * calculating the displacement in the instruction.
			     */
			    if(r_pcrel)
				offset += input_pc;
			    break;
			default:
			    /* the error check is catched below */
			    break;
			}
		    }
		}

		if((merged_symbol->nlist.n_type & N_TYPE) == N_SECT)
		    output_sections[merged_symbol->nlist.n_sect]->referenced =
									   TRUE;
	    }
	    else{
		/*
		 * This is a local relocation entry (the value to which the item
		 * to be relocated is refering to is defined in section number
		 * r_symbolnum).  So the address of that section in the input
		 * file is subtracted and the value of that section in the
		 * output is added to the item being relocated.
		 */
		value = 0;
		/*
		 * If the symbol is not in any section the value to be added to
		 * the item to be relocated is the zero above and any pc
		 * relative change in value added below.
		 */
		if(r_symbolnum == R_ABS){
		    /*
		     * If we are being called only to get the references for
		     * this relocation entry fill in it has none and return.
		     */
		    if(refs != NULL){
			refs->ref1.ref_type = LIVE_REF_NONE;
			refs->ref2.ref_type = LIVE_REF_NONE;
			return;
		    }
		}
		else{
		    if(r_symbolnum > cur_obj->nsection_maps){
			error_with_cur_obj("r_symbolnum (%lu) field of local "
			    "relocation entry %lu in section (%.16s,%.16s) "
			    "out of range", r_symbolnum, i,
			    section_map->s->segname, section_map->s->sectname);
			return;
		    }
		    local_map = &(cur_obj->section_maps[r_symbolnum - 1]);
		    local_map->output_section->referenced = TRUE;
		    pair_local_map = NULL;
		    if(r_type == ARM_RELOC_SECTDIFF ||
		       r_type == ARM_RELOC_LOCAL_SECTDIFF){
			pair_local_map =
			    &(cur_obj->section_maps[pair_r_symbolnum - 1]);
			pair_local_map->output_section->referenced = TRUE;
		    }
		    if(local_map->nfine_relocs == 0 && 
		       (pair_local_map == NULL ||
			pair_local_map->nfine_relocs == 0) ){
			if(r_type == ARM_RELOC_SECTDIFF ||
			   r_type == ARM_RELOC_LOCAL_SECTDIFF){
			    value = - local_map->s->addr
				    + (local_map->output_section->s.addr +
				       local_map->offset)
				    + pair_local_map->s->addr
				    - (pair_local_map->output_section->s.addr +
				       pair_local_map->offset);
			}
			else{
			    value = - local_map->s->addr
				    + (local_map->output_section->s.addr +
				       local_map->offset);
			}
		    }
		    else{
			/*
			 * For items to be relocated that refer to a section
			 * with fine relocation the value is set (not adjusted
			 * with addition).  So the new value is directly
			 * calculated from the old value.
			 */
			if(r_pcrel){
			    input_pc = section_map->s->addr +
				       r_address;
			    if(section_map->nfine_relocs == 0)
				output_pc = section_map->output_section->s.addr
					    + section_map->offset +
					    r_address;
			    else
				output_pc = section_map->output_section->s.addr
					    + 
					fine_reloc_output_offset(section_map,
								 r_address);
			}
			else{
			    input_pc = 0;
			    output_pc = 0;
			}
			/*
			 * Get the value of the expresion of the item to be
			 * relocated.
			 */
			if(r_type == ARM_RELOC_VANILLA ||
			   r_type == ARM_RELOC_SECTDIFF ||
			   r_type == ARM_RELOC_LOCAL_SECTDIFF){
			    switch(r_length){
			    case 0: /* byte */
				value = get_byte((char *)(contents +
							  r_address));
				break;
			    case 1: /* word (2 byte) */
				value = get_short((short *)(contents +
							    r_address));
				break;
			    case 2: /* long (4 byte) */
				value = get_long((long *)(contents +
							  r_address));
				break;
			    default:
				/* the error check is catched below */
				break;
			    }
			}
			else{
			    instruction = get_long((long *)(contents +
							    r_address));
			    switch(r_type){
			    case ARM_RELOC_BR24:
				value = instruction & 0x00ffffff;
				if((value & 0x00800000) != 0)
				    value |= 0xff000000;
				/* The value (displacement) is shifted by 2 */
				value = value << 2;
				/*
				 * For a BLX instruction, set bit[1] of the
				 * result to the H bit.
				 */
				if((instruction & 0xff000000) == 0xfb000000)
				    value |= 0x2;
				/* The pc added will be +8 from the pc */
				value += 8;
				break;
			    case ARM_THUMB_RELOC_BR22:
				/*
				 * The code below assumes ARM is little endian
				 * such that "the first 16-bit thumb
				 * instruction" is the low 16 bits and "the
				 * second 16-bit thumb instruction" is the high
				 * 16 bits of the 32-bits in the variable
				 * instruction.
				 */
				/* the first instruction has the upper eleven
				   bits of the two byte displacement */
				value = (instruction & 0x7FF) << 12;
				/* sign extend if needed */
				if((value & 0x400000) != 0)
				    value |= 0xFF800000;
				/* the second instruction has the lower eleven
				   bits of the two byte displacement.  Add that 
				   times two to get the target address */
				value += 2*((instruction >> 16) & 0x7FF);
				/* The pc added will be +4 from the pc */
				value += 4;
				/*
				 * For BLX, the resulting address is forced to
				 * be word-aligned by clearing bit[1].
				 */
				if(((instruction & 0x18000000) == 0x08000000) &&
				   ((input_pc + value) & 0x2))
				  value -= 2;
				break;
			    default:
				/* the error check is catched below */
				break;
			    }
			}
			if(r_type == ARM_RELOC_SECTDIFF ||
			   r_type == ARM_RELOC_LOCAL_SECTDIFF){
			    /*
			     * For ARM_RELOC_SECTDIFF's the item to be
			     * relocated, in value, is the value of the
			     * expression:
			     *     r_value - pair_r_value + offset
			     * To set the value of the relocated expression,
			     * it is set from relocating the two r_value's and
			     * adding back in the offset.  So here get the
			     * offset from the value of the expression.
			     */
			    value += input_pc; /* adjust for pcrel */
			    offset = value - r_value + pair_r_value;

			    /*
			     * If we are being called only to get the references
			     * for this relocation entry fill it in and return.
			     */
			    if(refs != NULL){
				fine_reloc_output_ref(
				    local_map,
				    r_value - local_map->s->addr,
				    &(refs->ref1) );
				fine_reloc_output_ref(
				    local_map,
				    pair_r_value - local_map->s->addr,
				    &(refs->ref2) );
				return;
			    }

			    /*
			     * Now build up the value of the relocated
			     * expression one part at a time.  First set the
			     * new value to the relocated r_value.
			     */
		    	    if(local_map->nfine_relocs != 0){
				/*
				 * Check to see if this reference is legal with
				 * respect to indirect sections.
				 */
				legal_reference(section_map, r_address,
				    local_map, r_value - local_map->s->addr,
				    i,
				    r_type != ARM_RELOC_LOCAL_SECTDIFF);
				value = fine_reloc_output_address(local_map,
					    r_value - local_map->s->addr,
					    local_map->output_section->s.addr);
			    }
			    else{
				value = local_map->output_section->s.addr +
					local_map->offset +
					r_value - local_map->s->addr;
			    }
			    /* Second subtract the relocated pair_r_value. */
			    if(pair_local_map->nfine_relocs != 0){
				/*
				 * Check to see if this reference is legal with
				 * respect to indirect sections.
				 */
				legal_reference(section_map, r_address,
				    pair_local_map, pair_r_value -
				    pair_local_map->s->addr, i, TRUE);
				value -=
				    fine_reloc_output_address(pair_local_map,
					pair_r_value - pair_local_map->s->addr,
				        pair_local_map->output_section->s.addr);
			    }
			    else{
				value -=
				    pair_local_map->output_section->s.addr +
				    pair_local_map->offset +
				    pair_r_value - pair_local_map->s->addr;
			    }
			    /* Third add in the offset. */
			    value += offset;
			    value -= output_pc; /* adjust for pcrel */
			}
			else{
			    /*
			     * If the relocation entry is not a scattered
			     * relocation entry then the relocation is based on
			     * the value of value of the expresion of the item
			     * to be relocated.  If it is a scattered relocation
			     * entry then the relocation is based on the r_value
			     * in the relocation entry and the offset part of
			     * the expression at the item to be relocated is
			     * extracted so it can be added after the relocation
			     * is done.
			     */
			    value += input_pc;
			    if(r_scattered == 0){
				r_value = value;
				offset = 0;
			    }
			    else{
				offset = value - r_value;
			    }
			    /*
			     * Check to see if this reference is legal with
			     * respect to indirect sections.
			     */
			    legal_reference(section_map, r_address, local_map,
				    r_value - local_map->s->addr + offset, i,
				    FALSE);

			    /*
			     * If we are being called only to get the references
			     * for this relocation entry fill it in and return.
			     */
			    if(refs != NULL){
				fine_reloc_output_ref(
				    local_map,
				    r_value - local_map->s->addr,
				    &(refs->ref1) );
				refs->ref2.ref_type = LIVE_REF_NONE;
				return;
			    }

			    value = fine_reloc_output_address(local_map,
					r_value - local_map->s->addr,
					local_map->output_section->s.addr);
			    value -= output_pc;
			    value += offset;
			}
			if(r_type == ARM_RELOC_VANILLA ||
			   r_type == ARM_RELOC_LOCAL_SECTDIFF ||
			   r_type == ARM_RELOC_SECTDIFF){
			    switch(r_length){
			    case 0: /* byte */
				if( (value & 0xffffff00) &&
				   ((value & 0xffffff80) != 0xffffff80))
				    error_with_cur_obj("relocation for entry "
					"%lu in section (%.16s,%.16s) does not "
					"fit in 1 byte", i,
					section_map->s->segname,
					section_map->s->sectname);
				set_byte((char *)(contents + r_address), value);
				break;
			    case 1: /* word (2 byte) */
				if( (value & 0xffff0000) &&
				   ((value & 0xffff8000) != 0xffff8000))
				    error_with_cur_obj("relocation for entry "
					"%lu in section (%.16s,%.16s) does not "
					"fit in 2 bytes", i,
					section_map->s->segname,
					section_map->s->sectname);
				set_short((short *)(contents + r_address),
					  value);
				break;
			    case 2: /* long (4 byte) */
				set_long((long *)(contents + r_address), value);
				break;
			    default:
				error_with_cur_obj("r_length field of "
				    "relocation entry %lu in section (%.16s,"
				    "%.16s) invalid", i,
				    section_map->s->segname,
				    section_map->s->sectname);
				return;
			    }
			}
			else{
			    switch(r_type){
			    case ARM_RELOC_BR24:
				/* The pc added will be +8 from the pc */
				value -= 8;
				/*
				 * An ARM BLX targetting an ARM symbol or
				 * a local symbol needs to be converted to a
				 * BL.  This could happen if it was originally
				 * targetting a thumb stub which will be
				 * optimized away.
				 */
				if(((fine_reloc_arm(local_map,
					r_value - local_map->s->addr) == TRUE) ||
				    (fine_reloc_local(local_map,
					r_value - local_map->s->addr) == TRUE)) &&
				   ((instruction & 0xfe000000) == 0xfa000000))
				    instruction = 0xeb000000;
				/*
				 * For arm branch instructions if the target is 
				 * a thumb symbol it must be converted to a
				 * branch and exchange instruction (unless it
				 * already is one).
				 */
				if((fine_reloc_thumb(local_map,
					r_value - local_map->s->addr) == TRUE) ||
				   ((instruction & 0xfe000000) == 0xfa000000)){
				    /*
				     * Only unconditional BL can be converted
				     * to BLX
				     */
				    if(((instruction & 0xff000000) != 0xeb000000) &&
				       ((instruction & 0xfe000000) != 0xfa000000))
					error_with_cur_obj("relocation error "
					    "for relocation entry %lu in "
					    "section (%.16s,%.16s) (branch "
					    "cannot be converted to BLX)", i,
					    section_map->s->segname,
					    section_map->s->sectname);
				    /*
				     * The H bit of the BLX instruction (bit 24)
				     * contains bit 1 of the target address.
				     */
				    instruction = (0xfa000000 |
						   ((value & 0x2) << 23));
				    /*
				     * This code assumes the thumb symbol
				     * address is two byte aligned.  This next
				     * line clears the last two bits so the next
				     * test will not cause an error
				     */
				    value &= ~0x2;
				}
				if((value & 0x3) != 0)
				    error_with_cur_obj("relocation error "
					"for relocation entry %lu in section "
					"(%.16s,%.16s) (displacement not a "
					"multiple of 4 bytes)", i,
					section_map->s->segname,
					section_map->s->sectname);
				if((value & 0xfe000000) != 0xfe000000 &&
				   (value & 0xfe000000) != 0x00000000)
				    error_with_cur_obj("relocation overflow "
					"for relocation entry %lu in section "
					"(%.16s,%.16s) (displacement too large)"
					, i, section_map->s->segname,
					section_map->s->sectname);
				instruction = (instruction & 0xff000000) |
					      ((value >> 2) & 0x00ffffff);
				break;
			    case ARM_THUMB_RELOC_BR22:
				/* The pc added will be +4 from the pc */
				value -= 4;
				/*
				 * Here we have a BL instruction targetting an
				 * arm symbol -- convert it to a BLX
				 */
				if((fine_reloc_arm(local_map,
					r_value - local_map->s->addr) == TRUE) &&
				   ((instruction & 0xf800f800) == 0xf800f000))
				    instruction &= 0xefffffff;
				/*
				 * Here we have a BLX instruction targetting a
				 * thumb symbol or a local symbol (which we will
				 * boldly assume to be thumb in the absence of
				 * any evidence to the contrary)  -- convert it
				 * to a BL
				 */
				if(((fine_reloc_thumb(local_map,
					r_value - local_map->s->addr) == TRUE) ||
				    (fine_reloc_local(local_map,
					r_value - local_map->s->addr) == TRUE)) &&
				   ((instruction & 0xf800f800) == 0xe800f000))
				    instruction |= 0x10000000;
				/* immediate must be multiple of four bytes.
				 * This enforces the requirement that
				 * instruction[0] must be zero for a BLX.
				 */
				if((instruction & 0xf800f800) == 0xe800f000 &&
				   (value & 0x2) != 0)
				    value += 2;
				instruction = (instruction & 0xf800f800) |
					      (value & 0x7ff000) >> 12 |
					      (value & 0xffe) << 15;
				break;
			    default:
				error_with_cur_obj("r_type field of "
				    "relocation entry %lu in section (%.16s,"
				    "%.16s) invalid", i,
				    section_map->s->segname,
				    section_map->s->sectname);
				return;
			    }
			    set_long((long *)(contents + r_address),
				     instruction);
			}

			goto update_reloc;
		    }
		}
	    }
	    if(r_pcrel){
		/*
		 * This is a relocation entry is also pc relative which means
		 * the value of the pc will get added to it when it is executed.
		 * The item being relocated has the value of the pc in the input
		 * file subtracted from it.  So to relocate this the value of
		 * pc in the input file is added and then value of the output
		 * pc is subtracted (since the offset into the section remains
		 * constant it is not added in and then subtracted out).
		 */
		if(section_map->nfine_relocs == 0)
		    value += + section_map->s->addr /* + r_address */
			     - (section_map->output_section->s.addr +
				section_map->offset /* + r_address */);
		else
		    value += + section_map->s->addr + r_address
			     - (section_map->output_section->s.addr +
			        fine_reloc_output_offset(section_map,
							 r_address));
	    }
	    if(r_type == ARM_RELOC_VANILLA ||
	       r_type == ARM_RELOC_LOCAL_SECTDIFF ||
	       r_type == ARM_RELOC_SECTDIFF){
		/*
		 * Pointers to thumb symbols must have their low bit set, but
		 * only after they have been relocated.  Code above determined
		 * if this is and external relocation entry for a thumb symbol
		 * that is being relocated and if so set 
		 * relocated_extern_thumb_symbol.  So now if this is a VANILLA
		 * relocation entry for a pointer set the low bit from
		 * relocated_extern_thumb_symbol.
		 */
		if(r_type == ARM_RELOC_VANILLA)
		    low_bit = (relocated_extern_thumb_symbol == TRUE) ? 1 : 0;
		else
		    low_bit = 0;
		/*
		 * This is part of the cctools_aek-thumb-hack branch.  It seems
		 * like a reasonable error check but I don't see how it could
		 * ever get triggered by any code going though the assember.
		 */
		if(r_type == ARM_RELOC_VANILLA && 
		   relocated_extern_thumb_symbol == TRUE && r_pcrel)
		    error_with_cur_obj("relocation for entry %lu in section "
			"(%.16s,%.16s) is VANILLA PC-relative to a thumb "
			"symbol %s", i, section_map->s->segname,
			section_map->s->sectname,
			merged_symbol->nlist.n_un.n_name);
		switch(r_length){
		case 0: /* byte */
		    value += get_byte((char *)(contents + r_address));
		    if( (value & 0xffffff00) &&
		       ((value & 0xffffff80) != 0xffffff80))
			error_with_cur_obj("relocation for entry %lu in section"
			    " (%.16s,%.16s) does not fit in 1 byte", i,
			    section_map->s->segname, section_map->s->sectname);
		    set_byte((char *)(contents + r_address), value | low_bit);
		    break;
		case 1: /* word (2 byte) */
		    value += get_short((short *)(contents + r_address));
		    if( (value & 0xffff0000) &&
		       ((value & 0xffff8000) != 0xffff8000))
			error_with_cur_obj("relocation for entry %lu in section"
			    " (%.16s,%.16s) does not fit in 2 bytes", i,
			    section_map->s->segname, section_map->s->sectname);
		    set_short((short *)(contents + r_address), value | low_bit);
		    break;
		case 2: /* long (4 byte) */
		    value += get_long((long *)(contents + r_address));
		    set_long((long *)(contents + r_address), value | low_bit);
		    break;
		default:
		    error_with_cur_obj("r_length field of relocation entry %lu "
			"in section (%.16s,%.16s) invalid", i,
			section_map->s->segname, section_map->s->sectname);
		    return;
		}
	    }
	    /*
	     * Do arm specific relocation based on the r_type.
	     */
	    else{
		instruction = get_long((long *)(contents + r_address));
		switch(r_type){
		case ARM_RELOC_BR24:
		    immediate = instruction & 0x00ffffff;
		    if((immediate & 0x00800000) != 0)
			immediate |= 0xff000000;
		    /* The value in the instruction is shifted by 2 */
		    immediate = immediate << 2;
		    /* In a BLX, bit 1 of the immediate is at bit 24
		     * of the instruction.
		     */
		    if((instruction & 0xfe000000) == 0xfa000000)
			immediate |= (instruction & 0x01000000) >> 23;
		    immediate += value;
		    /*
		     * Here we have a BLX instruction that targets an
		     * arm symbol -- convert it to a BL instruction.
		     */
		    if((r_extern == TRUE) &&
		       (relocated_extern_arm_symbol == TRUE) &&
		       ((instruction & 0xfe000000) == 0xfa000000))
			instruction = 0xeb000000;
		    /*
		     * For arm branch instructions if the target is a thumb 
		     * symbol it must be converted to a branch and exchange
		     * instruction (unless it already is one).
		     */
		    else if(((r_extern == TRUE) &&
			     (relocated_extern_thumb_symbol == TRUE)) ||
			    ((instruction & 0xfe000000) == 0xfa000000)){
			/* only unconditional BL can be converted to BLX */
			if(((instruction & 0xff000000) != 0xeb000000) &&
			   ((instruction & 0xfe000000) != 0xfa000000))
			    error_with_cur_obj("relocation error for relocation"
				" entry %lu in section (%.16s,%.16s) (branch "
				"cannot be converted to BLX)", i,
				section_map->s->segname,
				section_map->s->sectname);
			/* the H bit of the BLX instruction (bit 24) contains 
			   bit 1 of the target address */
			instruction = (0xfa000000 | ((immediate & 0x2) << 23));
			/* this code assumes the thumb symbol address is two
			   byte aligned.  This next line clears the last two
			   bits so the next test will not cause an error */
			immediate &= ~0x2;
		    }
		    if((immediate & 0x3) != 0)
			error_with_cur_obj("relocation error for relocation "
			    "entry %lu in section (%.16s,%.16s) (displacement "
			    "not a multiple of 4 bytes)", i,
			    section_map->s->segname, section_map->s->sectname);
		    if((immediate & 0xfe000000) != 0xfe000000 &&
		       (immediate & 0xfe000000) != 0x00000000)
			error_with_cur_obj("relocation overflow for relocation "
			    "entry %lu in section (%.16s,%.16s) (displacement "
			    "too large)", i, section_map->s->segname,
			    section_map->s->sectname);
		    instruction = (instruction & 0xff000000) |
		    		  (immediate & 0x03ffffff) >> 2;
		    break;
		case ARM_THUMB_RELOC_BR22:
		    /*
		     * The code below assumes ARM is little endian such that
		     * "the first 16-bit thumb instruction" is the low 16 bits
		     * and "the second 16-bit thumb instruction" is the high 16 
		     * bits of the 32-bits in the variable instruction.
		     */
		    /* the first instruction has the upper eleven bits of the
		       two byte displacement */
		    immediate = (instruction & 0x7FF) << 12;
		    /* sign extend if needed */
		    if((immediate & 0x400000) != 0)
			immediate |= 0xFF800000;
		    /* the second instruction has the lower eleven bits of the
		       two byte displacement.  Add that times two to get the
		       target address */
		    immediate += 2*((instruction >> 16) & 0x7FF);
		    /*
		     * For BLX, the resulting address is forced to be word-
		     * aligned by clearing bit[1].
		     */
		    if((instruction & 0xf800f800) == 0xe800f000 &&
		       (r_address & 0x2))
			immediate -= 2;
		    immediate += value;
		    /*
		     * The target address for a thumb branch must be to a
		     * two-byte address.
		     */
		    if((immediate & 0x1) != 0)
			error_with_cur_obj("relocation error for relocation "
			    "entry %lu in section (%.16s,%.16s) (displacement "
			    "not a multiple of 2 bytes)", i,
			    section_map->s->segname, section_map->s->sectname);
		    /*
		     * Here we have a BLX instruction that targets a
		     * thumb symbol -- convert it to a BL instruction.
		     */
		    if((r_extern == TRUE) &&
		       (relocated_extern_thumb_symbol == TRUE) &&
		       ((instruction & 0xf800f800) == 0xe800f000))
			instruction |= 0x10000000;
		    /*
		     * For thumb branch instructions if the target is not a
		     * thumb symbol (an arm symbol) it must be converted to a
		     * branch and exchange instruction (if it is not already
		     * one).
		     */
		    if(r_extern == TRUE &&
		       relocated_extern_arm_symbol == TRUE &&
		       (instruction & 0xf800f800) != 0xe800f000){
			/* Make sure we have a high+low BL */
			if((instruction & 0xf800f800) != 0xf800f000)
			    error_with_cur_obj("relocation error for relocation"
				" entry %lu in section (%.16s,%.16s) (unknown "
				"branch type)", i, section_map->s->segname, 
				section_map->s->sectname);
			/* Convert BL to BLX: clear top H bit of second insr */
			instruction &= 0xefffffff;
		    }
		    /* immediate must be multiple of four bytes.
		     * This enforces the requirement that
		     * instruction[0] must be zero for a BLX.
		     */
		    if((instruction & 0xf800f800) == 0xe800f000 &&
		       (immediate & 0x2) != 0)
			immediate += 2;
		    if((immediate & 0xffc00000) != 0xffc00000 &&
		       (immediate & 0xffc00000) != 0x00000000)
			error_with_cur_obj("relocation overflow for relocation "
			    "entry %lu in section (%.16s,%.16s) (displacement "
			    "too large)", i, section_map->s->segname,
			    section_map->s->sectname);
		    instruction = (instruction & 0xf800f800) |
				  (immediate & 0x7ff000) >> 12 |
				  (immediate & 0xffe) << 15;
		    break;
		case ARM_THUMB_32BIT_BRANCH:
		    break;
		default:
		    error_with_cur_obj("r_type field of relocation entry %lu "
			"in section (%.16s,%.16s) invalid", i,
			section_map->s->segname, section_map->s->sectname);
		    continue;
		}
		set_long((long *)(contents + r_address), instruction);
	    }

	    /*
	     * If relocation entries are to be saved in the output file then
	     * update the entry for the output file.
	     */
update_reloc:
	    ;
#ifndef RLD
	    if(save_reloc || output_for_dyld){
		if(r_extern){
		    /*
		     * If we are prebinding and this is a lazy pointer section
		     * change the relocation entry to a ARM_RELOC_PB_LA_PTR
		     * type.  This stuffs the value of the lazy pointer as it
		     * wouldn't be prebound in the r_value field.  So if the
		     * prebounding can't be used at runtime the value of the
		     * lazy pointer will get set back to the r_value by dyld.
		     */
		    if(prebinding == TRUE &&
		       (section_map->s->flags & SECTION_TYPE) ==
			S_LAZY_SYMBOL_POINTERS){
			sreloc = (struct scattered_relocation_info *)reloc;
			r_scattered = 1;
			sreloc->r_scattered = r_scattered;
			sreloc->r_address = r_address;
			sreloc->r_pcrel = r_pcrel;
			sreloc->r_length = r_length;
			sreloc->r_type = ARM_RELOC_PB_LA_PTR;
			sreloc->r_value = value;
		    }
		    /*
		     * For external relocation entries that the symbol is
		     * defined (not undefined or common) but not when we are
		     * forcing an external relocation entry for a global
		     * coalesced symbol and if the output file is not a multi
		     * module MH_DYLIB or the symbol is a private extern, it is
		     * changed to a local relocation entry using the section
		     * that symbol is defined in.  If still undefined or forcing
		     * an external relocation entry for a global coalesced
		     * symbol, then the index of the symbol in the output file
		     * is set into r_symbolnum.
		     */
		    else if((merged_symbol->nlist.n_type & N_TYPE) != N_UNDF &&
		            (merged_symbol->nlist.n_type & N_TYPE) != N_PBUD &&
		            force_extern_reloc == FALSE &&
		            ((filetype != MH_DYLIB ||
			      multi_module_dylib == FALSE) ||
			     (merged_symbol->nlist.n_type & N_PEXT) == N_PEXT)){
			reloc->r_extern = 0;
			/*
			 * If this symbol was in the base file then no futher
			 * relocation can ever be done (the symbols in the base
			 * file are fixed). Or if the symbol was an absolute
			 * symbol.
			 */
			if(merged_symbol->definition_object == base_obj ||
			   (merged_symbol->nlist.n_type & N_TYPE) == N_ABS){
				reloc->r_symbolnum = R_ABS;
			}
			else{
			    /*
			     * The symbol that this relocation entry is refering
			     * to is defined so convert this external relocation
			     * entry into a local or scattered relocation entry.
			     * If the item to be relocated has an offset added
			     * to the symbol's value and the output is not for
			     * dyld make it a scattered relocation entry else
			     * make it a local relocation entry.
			     */
			    if(offset == 0 || output_for_dyld){
				reloc->r_symbolnum =merged_symbol->nlist.n_sect;
			    }
			    else{
				sreloc = (struct scattered_relocation_info *)
					 reloc;
				r_scattered = 1;
				sreloc->r_scattered = r_scattered;
				sreloc->r_address = r_address;
				sreloc->r_pcrel = r_pcrel;
				sreloc->r_length = r_length;
				sreloc->r_type = r_type;
				sreloc->r_value = merged_symbol->nlist.n_value;
			    }
			}
		    }
		    else{
			reloc->r_symbolnum =
				      merged_symbol_output_index(merged_symbol);
		    }
		}
		else if(r_scattered == 0){
		    /*
		     * If we are prebinding and this is a lazy pointer section
		     * change the relocation entry to a ARM_RELOC_PB_LA_PTR
		     * type.  This stuffs the value of the lazy pointer as it
		     * wouldn't be prebound in the r_value field.  So if the
		     * prebounding can't be used at runtime the value of the
		     * lazy pointer will get set back to the r_value by dyld.
		     */
		    if(prebinding == TRUE &&
		       (section_map->s->flags & SECTION_TYPE) ==
			S_LAZY_SYMBOL_POINTERS){
			sreloc = (struct scattered_relocation_info *)reloc;
			r_scattered = 1;
			sreloc->r_scattered = r_scattered;
			sreloc->r_address = r_address;
			sreloc->r_pcrel = r_pcrel;
			sreloc->r_length = r_length;
			sreloc->r_type = ARM_RELOC_PB_LA_PTR;
			sreloc->r_value = value;
		    }
		    /*
		     * For local relocation entries the section number is
		     * changed to the section number in the output file.
		     */
		    else if(reloc->r_symbolnum != R_ABS){
			if(local_map->nfine_relocs == 0){
			    reloc->r_symbolnum =
				      local_map->output_section->output_sectnum;
			}
			else{
			    reloc->r_symbolnum =
				fine_reloc_output_sectnum(local_map,
						r_value - local_map->s->addr);
			}
		    }
		}
		else{
		    /*
		     * This is a scattered relocation entry.  If the output is
		     * for dyld convert it to a local relocation entry so as
		     * to not overflow the 24-bit r_address field in a scattered
		     * relocation entry.  The overflow would happen in
		     * reloc_output_for_dyld() in sections.c when it adjusts
		     * the r_address fields of the relocation entries.
		     */
		    if(output_for_dyld){
			reloc = (struct relocation_info *)sreloc;
			r_scattered = 0;
			reloc->r_address = r_address;
			reloc->r_pcrel = r_pcrel;
			reloc->r_extern = 0;
			reloc->r_length = r_length;
			reloc->r_type = r_type;
			if(local_map->nfine_relocs == 0){
			    reloc->r_symbolnum =
				      local_map->output_section->output_sectnum;
			}
			else{
			    reloc->r_symbolnum =
				fine_reloc_output_sectnum(local_map,
						r_value - local_map->s->addr);
			}
		    }
		    else{
			/*
			 * For scattered relocation entries the r_value field is
			 * relocated.
			 */
			if(local_map->nfine_relocs == 0)
			    sreloc->r_value +=
					   - local_map->s->addr
					   + local_map->output_section->s.addr +
					   local_map->offset;
			else
			    sreloc->r_value =
					fine_reloc_output_address(local_map,
						r_value - local_map->s->addr,
					   local_map->output_section->s.addr);
		    }
		}
		/*
		 * If this section that the reloation is being done for has fine
		 * relocation then the offset in the r_address field has to be
		 * set to where it will end up in the output file.  Otherwise
		 * it simply has to have the offset to where this contents
		 * appears in the output file. 
		 */
		if(r_scattered == 0){
		    if(section_map->nfine_relocs == 0){
			reloc->r_address += section_map->offset;
		    }
		    else{
			reloc->r_address = fine_reloc_output_offset(section_map,
								    r_address);
		    }
		}
		else{
		    if(section_map->nfine_relocs == 0){
			/* this can overflow the 24-bit sreloc->r_address */
			sreloc->r_address += section_map->offset;
		    }
		    else{
			sreloc->r_address =fine_reloc_output_offset(section_map,
								    r_address);
		    }
		}
		/*
		 * If their was a paired relocation entry then update the
		 * paired relocation entry.
		 */
		if(pair_r_type == ARM_RELOC_PAIR){
		    if(pair_reloc != NULL){
			/* I don't think arm has any pairs that are not
			   scattered relocs so this should never happen */
			pair_reloc->r_address = other_half;
		    }
		    else if(spair_reloc != NULL){
			if(r_type == ARM_RELOC_SECTDIFF ||
			   r_type == ARM_RELOC_LOCAL_SECTDIFF){
			    /*
			     * For ARM_RELOC_SECTDIFF relocation entries (which
			     * are always scattered types) the r_value field is
			     * relocated.
			     */
			    if(pair_local_map->nfine_relocs == 0)
				spair_reloc->r_value +=
				    - pair_local_map->s->addr
				    + (pair_local_map->output_section->s.addr +
				       pair_local_map->offset);
			    else
				spair_reloc->r_value =
				    fine_reloc_output_address(pair_local_map,
					pair_r_value - pair_local_map->s->addr,
					pair_local_map->output_section->s.addr);
			}
		    }
		    else{
			fatal("internal error, in arm_reloc() pair_r_type "
			    "is ARM_RELOC_PAIR but pair_reloc and spair_reloc "
			    "are NULL");
		    }
		}
	    }
#endif /* !defined(RLD) */
	    /*
	     * If their was a paired relocation entry then it has been processed
	     * so skip it by incrementing the index of the relocation entry that
	     * is being processed.
	     */
	    if(pair_r_type == ARM_RELOC_PAIR)
		i++;
	}
}
