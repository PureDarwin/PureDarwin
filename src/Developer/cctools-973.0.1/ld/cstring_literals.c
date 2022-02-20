/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * This file contains the routines that deal with literal 'C' string sections.
 * A string in this section must beable to me moved freely with respect to other
 * strings or data.  This means relocation must not reach outside the string and
 * things like: "abc"[i+20] can't be in this type of section.  Also strings
 * like: "foo\0bar" can not be in this type of section.
 */
#include <stdlib.h>
#if !(defined(KLD) && defined(__STATIC__))
#include <stdio.h>
#include <limits.h>
#include <mach/mach.h>
#else /* defined(KLD) && defined(__STATIC__) */
#include <mach/mach.h>
#include <mach/kern_return.h>
#define CHAR_MAX 0xff
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include <stdarg.h>
#include <string.h>
#include "stuff/openstep_mach.h"
#include <mach-o/loader.h>
#include "stuff/bool.h"
#include "stuff/bytesex.h"

#include "ld.h"
#include "live_refs.h"
#include "objects.h"
#include "sections.h"
#include "cstring_literals.h"
#include "pass2.h"
#include "hash_string.h"
#include "symbols.h"

/*
 * cstring_merge() merges cstring literals from the specified section in the
 * current object file (cur_obj).  When redo_live is FALSE it allocates a fine
 * relocation map and sets the fine_relocs field in the section_map to it (as
 * well as the count).  When redo_live is TRUE it re-merges only the live
 * cstrings based on the live bit in the previouly allocated fine_relocs.
 */
__private_extern__
void
cstring_merge(
struct cstring_data *data,
struct merged_section *ms,
struct section *s,
struct section_map *section_map,
enum bool redo_live)
{
    unsigned long ncstrings, i;
    char *cstrings, *p;
    struct fine_reloc *fine_relocs;
 
	if(s->size == 0){
	    if(redo_live == FALSE){
		section_map->fine_relocs = NULL;
		section_map->nfine_relocs = 0;
	    }
	    return;
	}
	/*
	 * Count the number of strings so the size of the fine relocation
	 * structures can be allocated.
	 */
	ncstrings = 0;
	cstrings = cur_obj->obj_addr + s->offset;
	if(*(cstrings + s->size - 1) != '\0'){
	    error_with_cur_obj("literal C string section (%.16s,%.16s) does "
			       "not end with a '\\0'", s->segname, s->sectname);
	    return;
	}
	for(p = cstrings; p < cstrings + s->size; p += strlen(p) + 1)
	    ncstrings++;
#ifdef DEBUG
	if(redo_live == FALSE){
	    data->nfiles++;
	    data->nbytes += s->size;
	    data->ninput_strings += ncstrings;
	}
#endif /* DEBUG */

	/*
	 * We will be called the first time with redo_live == FALSE and will
	 * just merge the cstrings from the input file and create the
	 * fine_relocs.
	 */
	if(redo_live == FALSE){
	    fine_relocs = allocate(ncstrings * sizeof(struct fine_reloc));
	    memset(fine_relocs, '\0', ncstrings * sizeof(struct fine_reloc));

	    /*
	     * lookup and enter each C string in the section and record the
	     * offsets in the input file and in the output file.
	     */
	    p = cstrings;
	    for(i = 0; i < ncstrings; i++){
		fine_relocs[i].input_offset = p - cstrings;
		fine_relocs[i].output_offset = lookup_cstring(p, data, ms);
		p += strlen(p) + 1;
	    }
	    section_map->fine_relocs = fine_relocs;
	    section_map->nfine_relocs = ncstrings;
	}
	else{
	    /*
	     * redo_live == TRUE and this is being called a second time after
	     * all the cstrings were previouly merged when -dead_strip is
	     * specified.  So now we walk the fine_relocs and only re-merge the
	     * live strings.
	     */
	    fine_relocs = section_map->fine_relocs;
	    ncstrings = section_map->nfine_relocs;
	    p = cstrings;
	    for(i = 0; i < ncstrings; i++){
		if(fine_relocs[i].live == TRUE){
		    fine_relocs[i].output_offset = lookup_cstring(p, data, ms);
		}
		else{
		    fine_relocs[i].output_offset = 0;
		}
		p += strlen(p) + 1;
	    }
	}
}

/*
 * cstring_order() enters cstring literals from the order_file from the merged
 * section structure.  Since this is called before any call to cstring_merge
 * and it enters the strings in the order of the file it causes the section
 * to be ordered.
 */
__private_extern__
void
cstring_order(
struct cstring_data *data,
struct merged_section *ms)
{
#ifndef RLD
    unsigned long i, line_number, line_length, max_line_length, output_offset;
    char *buffer;
    kern_return_t r;
    struct cstring_order_line *cstring_order_lines;
 
	/*
	 * Parse the load order file by changing '\n' to '\0'.  Also check for
	 * '\0 in the file and flag them as errors.  Also determine the maximum
	 * line length of the file for the needed buffer to allocate for
	 * character translation.
	 */
	line_number = 1;
	line_length = 1;
	max_line_length = 1;
	for(i = 0; i < ms->order_size; i++){
	    if(ms->order_addr[i] == '\0'){
		fatal("format error in -sectorder file: %s line %lu character "
		      "possition %lu for section (%.16s,%.16s) (illegal null "
		      "character \'\\0\' found)", ms->order_filename,
		      line_number, line_length, ms->s.segname, ms->s.sectname);
	    }
	    if(ms->order_addr[i] == '\n'){
		ms->order_addr[i] = '\0';
		if(line_length > max_line_length)
		    max_line_length = line_length;
		line_number++;
		line_length = 1;
	    }
	    else
		line_length++;
	}

	/*
	 * Allocate the buffer to translate the order file lines' escape
	 * characters into real characters.
	 */
	buffer = allocate(max_line_length + 1);

	/*
	 * If -dead_strip is specified allocate the needed structures so that
	 * the order of the live cstrings can be recreated later by
	 * cstring_reset_live().
	 */
	cstring_order_lines = NULL;
	if(dead_strip == TRUE){
	    data->cstring_load_order_data =
		allocate(sizeof(struct cstring_load_order_data));
	    cstring_order_lines = allocate(sizeof(struct cstring_order_line) *
					   (line_number - 1));
	    data->cstring_load_order_data->order_line_buffer =
		buffer;
	    data->cstring_load_order_data->cstring_order_lines =
		cstring_order_lines;
	    data->cstring_load_order_data->ncstring_order_lines =
		(line_number - 1);
	}

	/*
	 * Process each line in the order file by translating all escape
	 * characters and then entering the cstring using lookup_cstring().
	 * If -dead_strip is specified save away the starting character index
	 * of each order line and the output offset.
	 */
	line_number = 1;
	for(i = 0; i < ms->order_size; i++){
	    if(dead_strip == TRUE)
		cstring_order_lines[line_number - 1].character_index = i;

	    get_cstring_from_sectorder(ms, &i, buffer, line_number, 1);
	    output_offset = lookup_cstring(buffer, data, ms);

	    if(dead_strip == TRUE)
		cstring_order_lines[line_number - 1].output_offset =
		    output_offset;

	    line_number++;
	}

	/*
	 * If -dead_strip is not specified free up the memory for the line
	 * buffer and the load order file.  If -dead_strip is specified these
	 * will be free'ed up in cstring_reset_live().
	 */
	if(dead_strip == FALSE){

	    /* deallocate the line buffer */
	    free(buffer);

	    /*
	     * Deallocate the memory for the load order file now that it is
	     * nolonger needed (since the memory has been written on it is
	     * always deallocated so it won't get written to the swap file
	     * unnecessarily).
	     */
	    if((r = vm_deallocate(mach_task_self(), (vm_address_t)
		ms->order_addr, ms->order_size)) != KERN_SUCCESS)
		mach_fatal(r, "can't vm_deallocate() memory for -sectorder "
			   "file: %s for section (%.16s,%.16s)",
			   ms->order_filename, ms->s.segname,
			   ms->s.sectname);
	    ms->order_addr = NULL;
	}
#endif /* !defined(RLD) */
}

/*
 * cstring_reset_live() is called when -dead_strip is specified after all the
 * literals from the input objects are merged.  It clears out the cstring_data
 * so the live cstrings can be re-merged (by later calling cstring_merge() with
 * redo_live == TRUE.  In here we first merge in the live cstrings from the
 * order file if any. 
 */
__private_extern__
void
cstring_reset_live(
struct cstring_data *data,
struct merged_section *ms)
{
#ifndef RLD
    unsigned long i, ncstring_order_lines, character_index, line_number;
    char *buffer;
    struct cstring_order_line *cstring_order_lines;
    enum bool live;
    kern_return_t r;

	/* reset the merge section size back to zero */
	ms->s.size = 0;

	/* clear out the previously merged data */
	cstring_free(data);

	/*
	 * If this merged section has an order file we need to re-merged only
	 * the live cstrings from that order file.
	 */
	if(ms->order_filename != NULL){
	    buffer = data->cstring_load_order_data->order_line_buffer;
	    cstring_order_lines =
		data->cstring_load_order_data->cstring_order_lines;
	    ncstring_order_lines =
		data->cstring_load_order_data->ncstring_order_lines;
	    for(i = 0; i < ncstring_order_lines; i++){
		/*
		 * Figure out if this cstring order line's output_index is live
		 * and if so re-merge the cstring literal.
		 */
		live = is_literal_output_offset_live(
			ms, cstring_order_lines[i].output_offset);
		line_number = i + 1;
		if(live){
		    character_index = cstring_order_lines[i].character_index;
		    get_cstring_from_sectorder(ms, &character_index, buffer,
					       line_number, 1);
		    (void)lookup_cstring(buffer, data, ms);
		}
		else{
		    if(sectorder_detail == TRUE)
			warning("specification of string in -sectorder file: "
				"%s on line %lu for section (%.16s,%.16s) not "
				"used (dead stripped)", ms->order_filename,
				line_number, ms->s.segname, ms->s.sectname);
		}
	    }

	    /* deallocate the various data structures no longer needed */
	    free(data->cstring_load_order_data->order_line_buffer);
	    free(data->cstring_load_order_data->cstring_order_lines);
	    free(data->cstring_load_order_data);
	    data->cstring_load_order_data = NULL;

	    /*
	     * Deallocate the memory for the load order file now that it is
	     * nolonger needed (since the memory has been written on it is
	     * allways deallocated so it won't get written to the swap file
	     * unnecessarily).
	     */
	    if((r = vm_deallocate(mach_task_self(), (vm_address_t)
		ms->order_addr, ms->order_size)) != KERN_SUCCESS)
		mach_fatal(r, "can't vm_deallocate() memory for -sectorder "
			   "file: %s for section (%.16s,%.16s)",
			   ms->order_filename, ms->s.segname,
			   ms->s.sectname);
	    ms->order_addr = NULL;
	}
#endif /* !defined(RLD) */
}

/*
 * get_cstring_from_sectorder() parses a cstring from a order file for the
 * specified merged_section, ms, starting from the index, *index the order file
 * must have had its newlines changed to '\0's previouly.  It places the parsed
 * cstring in the specified buffer, buffer and advances the index over the
 * cstring it parsed.  line_number and char_pos are used for printing error
 * messages and refer the line_number and character possition the index is at.
 */
__private_extern__
void
get_cstring_from_sectorder(
struct merged_section *ms,
unsigned long *index,
char *buffer,
unsigned long line_number,
unsigned long char_pos)
{
#ifndef RLD
    unsigned long i, j, k, char_value;
    char octal[4], hex[9];

	j = 0;
	/*
	 * See that this is not the end of a line in the order file.
	 */
	for(i = *index; i < ms->order_size && ms->order_addr[i] != '\0'; i++){
	    /*
	     * See if this character the start of an escape sequence.
	     */
	    if(ms->order_addr[i] == '\\'){
		if(i + 1 >= ms->order_size || ms->order_addr[i + 1] == '\0')
		    fatal("format error in -sectorder file: %s line %lu "
			  "character possition %lu for section (%.16s,"
			  "%.16s) (\'\\\' at the end of the line)",
			  ms->order_filename, line_number, char_pos,
			  ms->s.segname, ms->s.sectname);
		/* move past the '\\' */
		i++;
		char_pos++;
		if(ms->order_addr[i] >= '0' && ms->order_addr[i] <= '7'){
		    /* 1, 2 or 3 octal digits */
		    k = 0;
		    octal[k++] = ms->order_addr[i];
		    char_pos++;
		    if(i+1 < ms->order_size &&
		       ms->order_addr[i+1] >= '0' &&
		       ms->order_addr[i+1] <= '7'){
			octal[k++] = ms->order_addr[++i];
			char_pos++;
		    }
		    if(i+1 < ms->order_size &&
		       ms->order_addr[i+1] >= '0' &&
		       ms->order_addr[i+1] <= '7'){
			octal[k++] = ms->order_addr[++i];
			char_pos++;
		    }
		    octal[k] = '\0';
		    char_value = strtol(octal, NULL, 8);
		    if(char_value > CHAR_MAX){
			error("format error in -sectorder file: %s line %lu "
			      "for section (%.16s,%.16s) (escape sequence"
			      " ending at character possition %lu out of "
			      "range for character)", ms->order_filename,
			      line_number, ms->s.segname, ms->s.sectname,
			      char_pos - 1);
		    }
		    buffer[j++] = (char)char_value;
		}
		else{
		    switch(ms->order_addr[i]){
		    case 'n':
			buffer[j++] = '\n';
			char_pos++;
			break;
		    case 't':
			buffer[j++] = '\t';
			char_pos++;
			break;
		    case 'v':
			buffer[j++] = '\v';
			char_pos++;
			break;
		    case 'b':
			buffer[j++] = '\b';
			char_pos++;
			break;
		    case 'r':
			buffer[j++] = '\r';
			char_pos++;
			break;
		    case 'f':
			buffer[j++] = '\f';
			char_pos++;
			break;
		    case 'a':
			buffer[j++] = '\a';
			char_pos++;
			break;
		    case '\\':
			buffer[j++] = '\\';
			char_pos++;
			break;
		    case '\?':
			buffer[j++] = '\?';
			char_pos++;
			break;
		    case '\'':
			buffer[j++] = '\'';
			char_pos++;
			break;
		    case '\"':
			buffer[j++] = '\"';
			char_pos++;
			break;
		    case 'x':
			/* hex digits */
			k = 0;
			while(i+1 < ms->order_size &&
			      ((ms->order_addr[i+1] >= '0' &&
				ms->order_addr[i+1] <= '9') ||
			       (ms->order_addr[i+1] >= 'a' &&
				ms->order_addr[i+1] <= 'f') ||
			       (ms->order_addr[i+1] >= 'A' &&
				ms->order_addr[i+1] <= 'F')) ){
			    if(k <= 8)
				hex[k++] = ms->order_addr[++i];
			    else
				++i;
			    char_pos++;
			}
			if(k > 8){
			    error("format error in -sectorder file: %s line"
				  " %lu for section (%.16s,%.16s) (hex "
				  "escape ending at character possition "
				  "%lu out of range)", ms->order_filename,
				  line_number, ms->s.segname,
				  ms->s.sectname, char_pos);
			    break;
			}
			hex[k] = '\0';
			char_value = strtol(hex, NULL, 16);
			if(char_value > CHAR_MAX){
			    error("format error in -sectorder file: %s line"
				  " %lu for section (%.16s,%.16s) (escape "
				  "sequence ending at character possition "
				  "%lu out of range for character)",
				  ms->order_filename, line_number,
				  ms->s.segname, ms->s.sectname, char_pos);
			}
			buffer[j++] = (char)char_value;
			char_pos++;
			break;
		    default:
			error("format error in -sectorder file: %s line %lu "
			      "for section (%.16s,%.16s) (unknown escape "
			      "sequence ending at character possition %lu)",
			      ms->order_filename, line_number,
			      ms->s.segname, ms->s.sectname, char_pos);
			buffer[j++] = ms->order_addr[i];
			char_pos++;
			break;
		    }
		}
	    }
	    /*
	     * This character is not the start of an escape sequence so take
	     * it as it is.
	     */
	    else{
		buffer[j] = ms->order_addr[i];
		char_pos++;
		j++;
	    }
	}
	buffer[j] = '\0';
	*index = i;
#endif /* !defined(RLD) */
}

/*
 * lookup_cstring() looks up the cstring passed to it in the cstring_data
 * passed to it and returns the offset the cstring will have in the output
 * file.  It creates the hash table as needed and the blocks to store the
 * strings and attaches them to the cstring_data passed to it.  The total
 * size of the section is accumulated in ms->s.size which is the merged
 * section for this literal section.  The string is aligned to the alignment
 * in the merged section (ms->s.align).
 */
__private_extern__
unsigned long
lookup_cstring(
char *cstring,
struct cstring_data *data,
struct merged_section *ms)
{
    unsigned long hashval, len, cstring_len;
    struct cstring_bucket *bp;
    struct cstring_block **p, *cstring_block;

	if(data->hashtable == NULL){
	    data->hashtable = allocate(sizeof(struct cstring_bucket *) *
				       CSTRING_HASHSIZE);
	    memset(data->hashtable, '\0', sizeof(struct cstring_bucket *) *
					  CSTRING_HASHSIZE);
	}
#if defined(DEBUG) && defined(PROBE_COUNT)
	    data->nprobes++;
#endif
	hashval = hash_string(cstring, NULL) % CSTRING_HASHSIZE;
	for(bp = data->hashtable[hashval]; bp; bp = bp->next){
	    if(strcmp(cstring, bp->cstring) == 0)
		return(bp->offset);
#if defined(DEBUG) && defined(PROBE_COUNT)
	    data->nprobes++;
#endif
	}

	cstring_len = strlen(cstring) + 1;
	len = rnd(cstring_len, 1 << ms->s.align);
	bp = allocate(sizeof(struct cstring_bucket));
	for(p = &(data->cstring_blocks); *p ; p = &(cstring_block->next)){
	    cstring_block = *p;
	    if(cstring_block->full)
		continue;
	    if(len > cstring_block->size - cstring_block->used){
		cstring_block->full = TRUE;
		continue;
	    }
	    strcpy(cstring_block->cstrings + cstring_block->used, cstring);
	    memset(cstring_block->cstrings + cstring_block->used + cstring_len,
		   '\0', len - cstring_len);
	    bp->cstring = cstring_block->cstrings + cstring_block->used;
	    cstring_block->used += len;
	    bp->offset = ms->s.size;
	    bp->next = data->hashtable[hashval];
	    data->hashtable[hashval] = bp;
	    ms->s.size += len;
#ifdef DEBUG
	    data->noutput_strings++;
#endif /* DEBUG */
	    return(bp->offset);
	}
	*p = allocate(sizeof(struct cstring_block));
	cstring_block = *p;
	cstring_block->size = (len > host_pagesize ? len : host_pagesize);
	cstring_block->used = len;
	cstring_block->full = (len == cstring_block->size ? TRUE : FALSE);
	cstring_block->next = NULL;
	cstring_block->cstrings = allocate(cstring_block->size);
	strcpy(cstring_block->cstrings, cstring);
	memset(cstring_block->cstrings + cstring_len, '\0', len - cstring_len);
	bp->cstring = cstring_block->cstrings;
	bp->offset = ms->s.size;
	bp->next = data->hashtable[hashval];
	data->hashtable[hashval] = bp;
	ms->s.size += len;
#ifdef DEBUG
	data->noutput_strings++;
#endif /* DEBUG */
	return(bp->offset);
}

/*
 * cstring_output() copies the cstrings for the data passed to it into the 
 * output file's buffer.  The pointer to the merged section passed to it is
 * used to tell where in the output file this section goes.  Then this routine
 * called cstring_free() to free() up all space used by this data block except
 * the data block itself.
 */
__private_extern__
void
cstring_output(
struct cstring_data *data,
struct merged_section *ms)
{
    unsigned long offset;
    struct cstring_block **p, *cstring_block;

	/*
	 * Copy the blocks into the output file.
	 */
	offset = ms->s.offset;
	for(p = &(data->cstring_blocks); *p ;){
	    cstring_block = *p;
	    memcpy(output_addr + offset,
		   cstring_block->cstrings,
		   cstring_block->used);
	    offset += cstring_block->used;
	    p = &(cstring_block->next);
	}
#ifndef RLD
	output_flush(ms->s.offset, offset - ms->s.offset);
#endif /* !defined(RLD) */
	cstring_free(data);
}

/*
 * cstring_free() free()'s up all space used by this cstring_data block except
 * the data block itself.
 */
__private_extern__
void
cstring_free(
struct cstring_data *data)
{
    unsigned long i;
    struct cstring_bucket *bp, *next_bp;
    struct cstring_block *cstring_block, *next_cstring_block;

	/*
	 * Free all data for this block.
	 */
	if(data->hashtable != NULL){
	    for(i = 0; i < CSTRING_HASHSIZE; i++){
		for(bp = data->hashtable[i]; bp; ){
		    next_bp = bp->next;
		    free(bp);
		    bp = next_bp;
		}
	    }
	    free(data->hashtable);
	    data->hashtable = NULL;
	}
	for(cstring_block = data->cstring_blocks; cstring_block ;){
	    next_cstring_block = cstring_block->next;
	    free(cstring_block->cstrings);
	    free(cstring_block);
	    cstring_block = next_cstring_block;
	}
	data->cstring_blocks = NULL;
}

#ifdef DEBUG
/*
 * print_cstring_data() prints a cstring_data.  Used for debugging.
 */
__private_extern__
void
print_cstring_data(
struct cstring_data *data,
char *indent)
{
    char *s;
    struct cstring_block **p, *cstring_block;
/*
    unsigned long i;
    struct cstring_bucket *bp;
*/

	print("%sC string data at 0x%x\n", indent, (unsigned int)data);
	if(data == NULL)
	    return;
	print("%s    hashtable 0x%x\n", indent,(unsigned int)(data->hashtable));
/*
	if(data->hashtable != NULL){
	    for(i = 0; i < CSTRING_HASHSIZE; i++){
		print("%s    %-3d [0x%x]\n", indent, i, data->hashtable[i]);
		for(bp = data->hashtable[i]; bp; bp = bp->next){
		    print("%s\tcstring %s\n", indent, bp->cstring);
		    print("%s\toffset  %lu\n", indent, bp->offset);
		    print("%s\tnext    0x%x\n", indent, bp->next);
		}
	    }
	}
*/
	print("%s   cstring_blocks 0x%x\n", indent,
	      (unsigned int)(data->cstring_blocks));
	for(p = &(data->cstring_blocks); *p ; p = &(cstring_block->next)){
	    cstring_block = *p;
	    print("%s\tsize %lu\n", indent, cstring_block->size);
	    print("%s\tused %lu\n", indent, cstring_block->used);
	    if(cstring_block->full)
		print("%s\tfull TRUE\n", indent);
	    else
		print("%s\tfull FALSE\n", indent);
	    print("%s\tnext 0x%x\n", indent,
		  (unsigned int)(cstring_block->next));
	    print("%s\tcstrings\n", indent);
	    for(s = cstring_block->cstrings;
	        s < cstring_block->cstrings + cstring_block->used;
	        s += strlen(s) + 1){
		print("%s\t    %s\n", indent, s);
	    }
	}
}

/*
 * cstring_data_stats() prints the cstring_data stats.  Used for tuning.
 */
__private_extern__
void
cstring_data_stats(
struct cstring_data *data,
struct merged_section *ms)
{
	if(data == NULL)
	    return;
	print("literal cstring section (%.16s,%.16s) contains:\n",
	      ms->s.segname, ms->s.sectname);
	print("    %u bytes of merged strings\n", ms->s.size);
	print("    from %lu files and %lu total bytes from those "
	      "files\n", data->nfiles, data->nbytes);
	print("    average number of bytes per file %g\n",
	      (double)((double)data->nbytes / (double)(data->nfiles)));
	print("    %lu merged strings\n", data->noutput_strings);
	print("    from %lu files and %lu total strings from those "
	      "files\n", data->nfiles, data->ninput_strings);
	print("    average number of strings per file %g\n",
	      (double)((double)data->ninput_strings / (double)(data->nfiles)));
	if(data->nprobes != 0){
	    print("    number of hash probes %lu\n", data->nprobes);
	    print("    average number of hash probes %g\n",
	    (double)((double)(data->nprobes) / (double)(data->ninput_strings)));
	}
}
#endif /* DEBUG */
