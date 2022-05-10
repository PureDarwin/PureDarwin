#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach-o/loader.h>
#include <stuff/bool.h>
#include <stuff/allocate.h>
#include <stuff/bytesex.h>
#include "dyld_bind_info.h"
#include "fixup-chains.h"
#include "stuff/guess_short_name.h"

/*****************************************************************************
 *
 *   LC_DYLD_INFO, LC_DYLD_INFO_ONLY
 *
 */

/* The entries of the ordinalTable for ThreadedRebaseBind. */
struct ThreadedBindData {
    const char* symbolName;
    int64_t addend;
    int libraryOrdinal;
    uint8_t flags;
    uint8_t type;
};

const char *
bindTypeName(
uint8_t type)
{
    switch(type){
        case BIND_TYPE_POINTER:
            return("pointer");
        case BIND_TYPE_TEXT_ABSOLUTE32:
            return("text abs32");
        case BIND_TYPE_TEXT_PCREL32:
            return("text rel32");
    }
    return("!!Unknown!!");
}
/*
static
const char *
ordinalName(
int libraryOrdinal,
const char **dylibs,
uint32_t ndylibs,
enum bool *libraryOrdinalSet)
{
    *libraryOrdinalSet = TRUE;
    switch(libraryOrdinal){
        case BIND_SPECIAL_DYLIB_SELF:
            return("this-image");
        case BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE:
            return("main-executable");
        case BIND_SPECIAL_DYLIB_FLAT_LOOKUP:
            return("flat-namespace");
        case BIND_SPECIAL_DYLIB_WEAK_LOOKUP:
            return("weak");
    }
    if(libraryOrdinal < BIND_SPECIAL_DYLIB_WEAK_LOOKUP){
        *libraryOrdinalSet = FALSE;
        return("Unknown special ordinal");
    }
    if(libraryOrdinal > ndylibs){
        *libraryOrdinalSet = FALSE;
        return("LibraryOrdinal out of range");
    }
    return(dylibs[libraryOrdinal-1]);
}
*/
/*
 * validateOrdinal validates a dylibOrdinal value as read from the LC_DYLD_INFO
 * opcodes. If dylibOrdinal is valid, validateOrdinal will return TRUE and set
 * dylibName to either the name of a dylib or to a human-readable special name.
 * If dylibOrdinal is not valid, it will return FALSE and set error to a
 * human-readable error string. error may be NULL.
 */
static
enum bool
validateOrdinal(
int dylibOrdinal,
const char **dylibs,
uint32_t ndylibs,
const char** dylibName,
const char** error)
{
    *dylibName = NULL;
    if (error)
        *error = NULL;

    if (dylibOrdinal < BIND_SPECIAL_DYLIB_WEAK_LOOKUP){
        if (error)
            *error = "Unknown special ordinal";
        return FALSE;
    }
    if (dylibOrdinal > 0 && dylibOrdinal > ndylibs){
        if (error)
            *error = "dylibOrdinal out of range";
        return FALSE;
    }

    switch(dylibOrdinal){
        case BIND_SPECIAL_DYLIB_SELF:
            *dylibName = "this-image";
            break;
        case BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE:
            *dylibName = "main-executable";
            break;
        case BIND_SPECIAL_DYLIB_FLAT_LOOKUP:
            *dylibName = "flat-namespace";
            break;
        case BIND_SPECIAL_DYLIB_WEAK_LOOKUP:
            *dylibName = "weak";
            break;
        default:
            *dylibName = dylibs[dylibOrdinal-1];
    }

    return TRUE;
}

/*
 * validateSegIndex validates a segIndex value as read from the LC_DYLD_INFO
 * opcodes. If segIndex is in range of the segs64 or segs arrays,
 * validateSegIndex will return TRUE. Otherwise, it will return FALSE and set
 * error to a human-readable error string.
 */
static
enum bool
validateSegIndex(
uint8_t segIndex,
struct segment_command **segs,
uint32_t nsegs,
struct segment_command_64 **segs64,
uint32_t nsegs64,
const char** error)
{
    *error = NULL;
    if(segs64 != NULL && segIndex >= nsegs64) {
        *error = "segment index out of range";
        return FALSE;
    }
    if(segs != NULL && segIndex >= nsegs) {
        *error = "segment index out of range";
        return FALSE;
    }
    return TRUE;
}

uint64_t
segStartAddress(
uint8_t segIndex,
struct segment_command **segs,
uint32_t nsegs,
struct segment_command_64 **segs64,
uint32_t nsegs64)
{
    if(segs != NULL){
        if(segIndex >= nsegs)
            return(0); /* throw "segment index out of range"; */
        return(segs[segIndex]->vmaddr);
    }
    else if(segs64 != NULL){
        if(segIndex >= nsegs64)
            return(0); /* throw "segment index out of range"; */
        return(segs64[segIndex]->vmaddr);
    }
    return(0);
}

const char *
segmentName(
uint8_t segIndex,
struct segment_command **segs,
uint32_t nsegs,
struct segment_command_64 **segs64,
uint32_t nsegs64)
{
    if(segs != NULL){
        if(segIndex >= nsegs)
            return("??"); /* throw "segment index out of range"; */
        return(segs[segIndex]->segname);
    }
    else if(segs64 != NULL){
        if(segIndex >= nsegs64)
            return("??"); /* throw "segment index out of range"; */
        return(segs64[segIndex]->segname);
    }
    return("??");
}

const char *
sectionName(
uint8_t segIndex,
uint64_t address,
struct segment_command **segs,
uint32_t nsegs,
struct segment_command_64 **segs64,
uint32_t nsegs64)
{
    struct section *s;
    struct section_64 *s64;
    uint32_t i;
    
    if(segs != NULL){
        if(segIndex >= nsegs)
            return("??"); /* throw "segment index out of range"; */
        
        s = (struct section *)((char *)segs[segIndex] +
                               sizeof(struct segment_command));
        for(i = 0; i < segs[segIndex]->nsects; i++){
            if(s->addr <= address && address < s->addr + s->size)
                return(s->sectname);
            s++;
        }
    }
    else if(segs64 != NULL){
        if(segIndex >= nsegs64)
            return("??"); /* throw "segment index out of range"; */
        
        s64 = (struct section_64 *)((char *)segs64[segIndex] +
                                    sizeof(struct segment_command_64));
        for(i = 0; i < segs64[segIndex]->nsects; i++){
            if(s64->addr <= address && address < s64->addr + s64->size)
                return(s64->sectname);
            s64++;
        }
    }
    return("??");
}

/*
 * segmentIndexFromAddr returns the index of a segment that contains the
 * specified input address. The address is a vmaddr, not a fileoff. Canonical
 * Mach-O files will include either 32-bit or 64-bit segments. If for some
 * reason both 32- and 64-bit segments are passed in here, 64-bit segments will
 * be consulted first; but really, you'll have other issues ...
 *
 * Earlier Mach-O structures encoded segment indicies directly. dyld3's
 * chained fixups format saves space by ommitting this information, knowing
 * that with a quick O(N) algorithm one can simply compute this information
 * from a vmaddr.
 */
static uint32_t
segmentIndexFromAddr(uint64_t addr,
                     struct segment_command **segs,
                     uint32_t nsegs,
                     struct segment_command_64 **segs64,
                     uint32_t nsegs64)
{
    for (uint32_t i = 0; i < nsegs64; ++i) {
        if (segs64[i]->vmaddr <= addr &&
            segs64[i]->vmaddr + segs64[i]->vmsize > addr)
            return i;
    }
    for (uint32_t i = 0; i < nsegs; ++i) {
        if (segs[i]->vmaddr <= addr &&
            segs[i]->vmaddr + segs[i]->vmsize > addr)
            return i;
    }

    return ((uint32_t)-1);
}

static uint64_t segmentFileOffset(
uint8_t segIndex,
struct segment_command **segs,
uint32_t nsegs,
struct segment_command_64 **segs64,
uint32_t nsegs64)
{
    if (segIndex < nsegs64)
        return segs64[segIndex]->fileoff;
    if (segIndex < nsegs)
        return segs[segIndex]->fileoff;
    return -1;
}

const char *
checkSegAndOffset(
int segIndex,
uint64_t segOffset,
struct segment_command **segs,
uint32_t nsegs,
struct segment_command_64 **segs64,
uint32_t nsegs64,
enum bool endInvalid)
{
    uint64_t address;
    
    if(segIndex == -1)
        return("missing preceding BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB");
    if(segs != NULL){
        if(segIndex >= nsegs)
            return("bad segIndex (too large)");
        address = segs[segIndex]->vmaddr + segOffset;
        if(address > segs[segIndex]->vmaddr + segs[segIndex]->vmsize)
            return("bad segOffset, too large");
        if(endInvalid == TRUE &&
           address == segs[segIndex]->vmaddr + segs[segIndex]->vmsize)
            return("bad segOffset, too large");
    }
    else if(segs64 != NULL){
        if(segIndex >= nsegs64)
            return("bad segIndex (too large)");
        address = segs64[segIndex]->vmaddr + segOffset;
        if(address > segs64[segIndex]->vmaddr + segs64[segIndex]->vmsize)
            return("bad segOffset, too large");
        if(endInvalid == TRUE &&
           address == segs64[segIndex]->vmaddr + segs64[segIndex]->vmsize)
            return("bad segOffset, too large");
    }
    return(NULL);
}

const char *
checkCountAndSkip(
uint32_t *count,
uint64_t skip,
int segIndex,
uint64_t segOffset,
struct segment_command **segs,
uint32_t nsegs,
struct segment_command_64 **segs64,
uint32_t nsegs64)
{
    uint64_t address, i;
    
    i = 0;
    if(segs != NULL){
        if(segIndex >= nsegs){
            *count = 1;
            return("bad segIndex (too large)");
        }
        if(*count > 1)
            i = (skip + 4) * (*count - 1);
        address = segs[segIndex]->vmaddr + segOffset;
        if(address >= segs[segIndex]->vmaddr + segs[segIndex]->vmsize){
            *count = 1;
            return("bad segOffset, too large");
        }
        if(address + i >= segs[segIndex]->vmaddr + segs[segIndex]->vmsize){
            *count = 1;
            return("bad count and skip, too large");
        }
    }
    else if(segs64 != NULL){
        if(segIndex >= nsegs64){
            *count = 1;
            return("bad segIndex (too large)");
        }
        if(*count > 1)
            i = (skip + 8) * (*count - 1);
        address = segs64[segIndex]->vmaddr + segOffset;
        if(address >= segs64[segIndex]->vmaddr + segs64[segIndex]->vmsize){
            *count = 1;
            return("bad segOffset, too large");
        }
        if(address + i >=
           segs64[segIndex]->vmaddr + segs64[segIndex]->vmsize){
            *count = 1;
            return("bad count and skip, too large");
        }
    }
    return(NULL);
}

static
uint64_t
read_uleb128(
const uint8_t **pp,
const uint8_t* end,
const char **error)
{
    const uint8_t *p = *pp;
    uint64_t result = 0;
    int bit = 0;
    
    *error = NULL;
    do{
        if(p >= end){
            *pp = p;
            *error = "malformed uleb128, extends past opcode bytes";
            return(0);
        }
        
        uint64_t slice = *p & 0x7f;
        
        if(bit >= 64 || slice << bit >> bit != slice){
            *pp = p;
            *error = "uleb128 too big for uint64";
            return(0);
        }
        else {
            result |= (slice << bit);
            bit += 7;
        }
    }while(*p++ & 0x80);
    *pp = p;
    return(result);
}

static
int64_t
read_sleb128(
const uint8_t **pp,
const uint8_t* end,
const char **error)
{
    const uint8_t *p = *pp;
    int64_t result = 0;
    int bit = 0;
    uint8_t byte;
    
    *error = NULL;
    do{
        if(p == end){
            *pp = p;
            *error = "malformed sleb128, extends past opcode bytes";
            return(0);
        }
        byte = *p++;
        result |= (((int64_t)(byte & 0x7f)) << bit);
        bit += 7;
    }while (byte & 0x80);
    // sign extend negative numbers
    if((byte & 0x40) != 0)
        result |= (-1LL) << bit;
    *pp = p;
    return(result);
}

static inline
void
printErrorBind(
int pass,
enum bool verbose,
const char* opName,
unsigned long location,
const char* error,
uint32_t *errorCount)
{
    if (1 == pass && verbose && error) {
        printf("bad bind info (for %s opcode at 0x%lx) %s\n",
               opName, location, error);
    }
    if (errorCount)
       (*errorCount)++;
}

static inline
void
printErrorOrdinal(
int pass,
enum bool verbose,
const char* opName,
unsigned long location,
int32_t ordinal,
uint32_t ndylibs,
const char* error,
uint32_t *errorCount)
{
    if (1 == pass && verbose && error) {
        printf("bad bind info (for %s opcode at 0x%lx) bad library ordinal "
               "%u (max %u): %s\n",
               opName, location, ordinal, ndylibs, error);
    }
    if (errorCount)
       (*errorCount)++;
}

static inline
void
printErrorOrdAddr(
                  int pass,
                  enum bool verbose,
                  const char* opName,
                  unsigned long location,
                  int32_t ordinal,
                  uint64_t ordAddr,
                  uint32_t *errorCount)
{
    if (1 == pass && verbose) {
        printf("bad bind info (for %s opcode at 0x%lx) bad ordinal: %u in "
               "pointer at address 0x%llx\n",
               opName, location, ordinal, ordAddr);
    }
    if (errorCount)
       (*errorCount)++;
}

static inline
void
printErrorBindType(
int pass,
enum bool verbose,
const char* opName,
unsigned long location,
uint8_t bindType,
uint32_t *errorCount)
{
    if (1 == pass && verbose) {
        printf("bad bind info (for %s opcode at 0x%lx) "
               "bad bind type: %u\n",
               opName, location, bindType);
    }
    if (errorCount)
       (*errorCount)++;
}

static inline
void
printErrorOffset(
int pass,
enum bool verbose,
const char* opName,
unsigned long location,
uint64_t address,
int reason,
uint32_t *errorCount)
{
    if (1 == pass && verbose) {
        const char* error = "invalid";
        if (0 == reason)
            error = "past end of file";
        else if (1 == reason)
            error = "past end of the same page";
        printf("bad bind info (for %s opcode at: 0x%lx) offset to next pointer "
               "in the chain after one at address 0x%llx is %s\n",
               opName, location, address, error);
    }
    if (errorCount)
       (*errorCount)++;
}

static inline
void
printErrorOpcode(
int pass,
enum bool verbose,
const char* opName,
unsigned long location,
uint8_t opcode,
uint32_t *errorCount)
{
    if (1 == pass && verbose) {
        if (opName)
            printf("bad bind info (for %s opcode at 0x%lx) bad sub-opcode "
                   "value 0x%x\n",
                   opName, location, opcode);
        else
            printf("bad bind info (for opcode at 0x%lx) bad opcode "
                   "value 0x%x\n",
                   location, opcode);
    }
    if (errorCount)
       (*errorCount)++;
}

static inline
void
printErrorFixupBindOrdinal(
uint64_t location,
uint32_t ordinal,
uint32_t count,
uint32_t *errorCount)
{
    printf("bad chained fixups: bind 0x%llx target ordinal %d "
           "exceeds target table size %d\n", location, ordinal, count);
    if (errorCount)
       (*errorCount)++;
}

/*
 * get_dyld_bind_info() unpacks the dyld bind info from the data from an
 * LC_BIND_INFO load command pointed to by start through end into the internal
 * dyld_bind_info structs returned through dbi, and its count ndbi.  The array
 * of dylib names and their count are passed in in dylibs and ndylibs.  The
 * array of segments (either 32-bit with segs & nsegs or 64-bit segs64 & nsegs64)
 * are used to determine which sections the pointers are in.
 *
 * A word about libraryOrdinal. The libraryOrdinal is a combination of array
 * ordinals and special magic values:
 *
 *       -3     BIND_SPECIAL_DYLIB_WEAK_LOOKUP
 *       -2     BIND_SPECIAL_DYLIB_FLAT_LOOKUP
 *       -1     BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE
 *        0     BIND_SPECIAL_DYLIB_SELF
 *   [1..n]     library ordinal
 *
 * The libraryOrdinal is commonly used to index into a C-array (that is, in
 * the range [0, n-1]) of dylibs. Ordinarily, once libraryOrdinal is assigned
 * one should call ordinalName to validate the ordinal. ordinalName will either
 * set libraryOrdinalSet to TRUE and return a human-readable library or special
 * name, or it will set libraryOrdinalSet to FALSE and return a human-readable
 * error message. While all this sounds perfectly reasonable, there is a
 * potential for mischief here ...
 */
void
get_dyld_bind_info(
const uint8_t* start, /* inputs */
const uint8_t* end,
const char **dylibs,
uint32_t ndylibs,
struct segment_command **segs,
uint32_t nsegs,
struct segment_command_64 **segs64,
uint32_t nsegs64,
enum bool swapped,
char *object_addr,
uint64_t object_size,
struct dyld_bind_info **dbi, /* output */
uint64_t *ndbi,
enum chain_format_t *chain_format,
enum bool print_errors)
{
    const uint8_t *p, *opcode_start;
    uint8_t type;
    int segIndex;
    uint64_t segOffset;
    const char* symbolName;
    const char* fromDylib;
    enum bool libraryOrdinalSet;
    int libraryOrdinal;
    int64_t addend;
    int64_t flags;
    uint32_t count;
    uint32_t skip;
    uint64_t segStartAddr;
    const char* segName;
    const char* weak_import;
    enum bool done = FALSE;
    const char *sectName, *error;
#define MAXERRORCOUNT 20
    uint32_t pass, errorCount;
    uint64_t n;
    uint32_t sizeof_pointer;
    char *pointerLocation;
    uint64_t offset, ordinalTableCount, ordinalTableIndex, delta;
    struct ThreadedBindData *ordinalTable;
    uint16_t ordinal;
    uint64_t pointerAddress, pointerPageStart;
    const char* opName;
    
    *chain_format = CHAIN_FORMAT_NONE;
    *dbi = NULL;
    *ndbi = 0;

    if (start == NULL) {
        printf("missing dyld bind info.\n");
        return;
    }
    
    ordinalTable = NULL;
    ordinalTableCount = 0;
    sizeof_pointer = segs ? 4 : 8;
    errorCount = 0;
    n = 0;
    for(pass = 1; pass <= 2; pass++){
        p = start;
        type = 0;
        segIndex = -1;
        segOffset = 0;
        symbolName = NULL;
        fromDylib = NULL;
        libraryOrdinalSet = FALSE;
        libraryOrdinal = 0;
        addend = 0;
        segStartAddr = 0;
        segName = NULL;
        sectName = NULL;
        weak_import = "";
        done = FALSE;
        ordinalTableIndex = (uint64_t)-1;
        flags = 0;
        if(errorCount >= MAXERRORCOUNT){
            if(print_errors)
                printf("too many bind info errors, giving up.\n");
            *dbi = NULL;
            *ndbi = 0;
            if(ordinalTable != NULL)
                free(ordinalTable);
            return;
        }
        if(pass == 2){
            *dbi = (struct dyld_bind_info *)
            allocate(n * sizeof(struct dyld_bind_info));
            memset(*dbi, 0, n * sizeof(struct dyld_bind_info));
            *ndbi = n;
            n = 0;
        }
        while(!done && (p < end) && errorCount < MAXERRORCOUNT){
            error = NULL;
            opcode_start = p;
            uint8_t immediate = *p & BIND_IMMEDIATE_MASK;
            uint8_t opcode = *p & BIND_OPCODE_MASK;
            opcode = *p & BIND_OPCODE_MASK;
            ++p;
            switch(opcode){
                case BIND_OPCODE_DONE:
                    done = TRUE;
                    break;
                case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
                    /*
                     * consume: immediate
                     * produce: libraryOrdinal, libraryOrdinalSet, fromDylib
                     */
                    opName = "BIND_OPCODE_SET_DYLIB_ORDINAL_IMM";
                    libraryOrdinal = immediate;
                    libraryOrdinalSet = validateOrdinal(libraryOrdinal,
                                                        dylibs, ndylibs,
                                                        &fromDylib, &error);
                    if (FALSE == libraryOrdinalSet)
                        printErrorOrdinal(pass, print_errors, opName,
                                          opcode_start - start,
                                          libraryOrdinal, ndylibs,
                                          error, &errorCount);
                    break;
                case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
                    /*
                     * consume: p, end
                     * produce: libraryOrdinal, libraryOrdinalSet, fromDylib,
                     *          error
                     */
                    opName = "BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB";
                    libraryOrdinal = (int)read_uleb128(&p, end, &error);
                    if (error) {
                        libraryOrdinalSet = FALSE;
                    }
                    else {
                        libraryOrdinalSet = validateOrdinal(libraryOrdinal,
                                                            dylibs, ndylibs,
                                                            &fromDylib, &error);
                    }
                    if (FALSE == libraryOrdinalSet)
                        printErrorOrdinal(pass, print_errors, opName,
                                          opcode_start - start,
                                          libraryOrdinal, ndylibs,
                                          error, &errorCount);
                    break;
                case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
                    /*
                     * consume: immediate
                     * produce: libraryOrdinal, libraryOrdinalSet, fromDylib
                     *
                     * the special ordinals are in the range of [-3, 0]
                     */
                    opName = "BIND_OPCODE_SET_DYLIB_SPECIAL_IMM";
                    if (immediate == 0) {
                        libraryOrdinal = 0;
                    }
                    else {
                        int8_t signExtended = BIND_OPCODE_MASK | immediate;
                        libraryOrdinal = signExtended;
                    }
                    libraryOrdinalSet = validateOrdinal(libraryOrdinal,
                                                        dylibs, ndylibs,
                                                        &fromDylib, &error);
                    if (FALSE == libraryOrdinalSet)
                        printErrorOrdinal(pass, print_errors, opName,
                                          opcode_start - start,
                                          libraryOrdinal, ndylibs,
                                          error, &errorCount);
                    break;
                case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
                    /*
                     * consume: p, end, immediate
                     * produce: p, symbolName, flags, weak_import
                     */
                    opName = "BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM";
                    weak_import = "";
                    symbolName = (char*)p;
                    while(*p != '\0' && (p < end))
                        ++p;
                    if(p == end){
                        error = "symbol name extends past opcodes";
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                        /*
                         * Even though the name does not end with a '\0' it will
                         * not be used as it is past the opcodes so there can't
                         * be a BIND opcode that follows that will use it.
                         */
                    }
                    else {
                        /* string terminator */
                        ++p;
                    }
                    flags = immediate;
                    if((flags & BIND_SYMBOL_FLAGS_WEAK_IMPORT) != 0)
                        weak_import = " (weak import)";
                    break;
                case BIND_OPCODE_SET_TYPE_IMM:
                    /*
                     * consume: immediate
                     * produce: type
                     */
                    opName = "BIND_OPCODE_SET_TYPE_IMM";
                    if (immediate == 0 || immediate > BIND_TYPE_TEXT_PCREL32) {
                        printErrorBindType(pass, print_errors, opName,
                                           opcode_start - start,
                                           immediate, &errorCount);
                        type = 0;
                    }
                    else {
                        type = immediate;
                    }
                    break;
                case BIND_OPCODE_SET_ADDEND_SLEB:
                    /*
                     * consume: p, end
                     * produce: addend, error
                     */
                    opName = "BIND_OPCODE_SET_ADDEND_SLEB";
                    addend = read_sleb128(&p, end, &error);
                    if (error) {
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                        addend = 0;
                    }
                    break;
                case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
                    /*
                     * consume: immediate
                     * produce: segIndex, segStartAddr, segName, segOffset,
                     *          error
                     */
                    opName = "BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB";
                    if (validateSegIndex(immediate, segs, nsegs,
                                         segs64, nsegs64, &error))
                    {
                        segOffset = read_uleb128(&p, end, &error);
                    }
                    if (!error) {
                        segIndex = immediate;
                        segStartAddr = segStartAddress(segIndex, segs, nsegs,
                                                       segs64, nsegs64);
                        segName = segmentName(segIndex, segs, nsegs,
                                              segs64, nsegs64);
                        error = checkSegAndOffset(segIndex, segOffset,
                                                  segs, nsegs,
                                                  segs64, nsegs64, TRUE);
                    }
                    if (error) {
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                        segIndex = -1;
                        segStartAddr = 0;
                        segName = NULL;
                        segOffset = 0;
                        error = NULL;
                    }
                    break;
                case BIND_OPCODE_ADD_ADDR_ULEB:
                    /*
                     * consume: p, end, segOffset
                     * produce: segOffset, error
                     */
                    opName = "BIND_OPCODE_ADD_ADDR_ULEB";
                    error = NULL;
                    if (-1 == segIndex) {
                        error = "segment index is invalid";
                    }
                    if (!error) {
                        segOffset += read_uleb128(&p, end, &error);
                    }
                    if (!error) {
                        error = checkSegAndOffset(segIndex, segOffset,
                                                  segs, nsegs,
                                                  segs64, nsegs64, TRUE);
                    }
                    if (error) {
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                        segIndex = -1;
                        segName = NULL;
                        segStartAddr = 0;
                        segOffset = 0;
                        error = NULL;
                    }
                    break;
                case BIND_OPCODE_DO_BIND:
                    /*
                     * consume: segIndex, segOffset, symbolName,
                     *          libraryOrdinalSet, ordinalTableIndex,
                     *          ordinalTableCount, ordinalTable, libraryOrdinal,
                     *          addend, flags, type, segName, segStartAddr,
                     *          fromDylib, weak_import
                     * produce: sectName, segOffset, dbi
                     */
                    opName = "BIND_OPCODE_DO_BIND";
                    /* start by doing all of the validations */
                    if(CHAIN_FORMAT_NONE == *chain_format)
                    {
                        error = checkSegAndOffset(segIndex, segOffset, segs,
                                                  nsegs, segs64, nsegs64, TRUE);
                        if (error) {
                            printErrorBind(pass, print_errors, opName,
                                           opcode_start-start, error,
                                           &errorCount);
                            segIndex = -1;
                            segName = NULL;
                            segStartAddr = 0;
                            segOffset = 0;
                            sectName = NULL;
                        }
                        else {
                            sectName = sectionName(segIndex,
                                                   segStartAddr + segOffset,
                                                   segs, nsegs,
                                                   segs64, nsegs64);
                        }
                    }
                    if (NULL == symbolName) {
                        error = "missing preceding BIND_OPCODE_SET_SYMBOL_"
                            "TRAILING_FLAGS_IMM opcode";
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                    }
                    if (FALSE == libraryOrdinalSet){
                        error = "missing preceding BIND_OPCODE_SET_DYLIB_"
                            "ORDINAL_* opcode";
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                    }
                    if(CHAIN_FORMAT_ARM64E == *chain_format){
                        /*
                         * At this point ordinalTableIndex should not equal
                         * ordinalTableCount or we have seen too many
                         * BIND_OPCODE_DO_BIND opcodes and that does not match
                         * the ordinalTableCount.
                         */
                        if(ordinalTableIndex >= ordinalTableCount){
                            error = "incorrect ordinal table size: number of "
                            "BIND_OPCODE_DO_BIND opcodes exceed the count in "
                            "previous BIND_SUBOPCODE_THREADED_SET_BIND_"
                            "ORDINAL_TABLE_SIZE_ULEB opcode";
                            printErrorBind(1, print_errors, opName,
                                           opcode_start-start, error,
                                           &errorCount);
                        }
                        else {
                            /*
                             * The ordinalTable needs to be built if possible.
                             * If the previous check failed, ordinalTable[oti]
                             * is not at all valid.
                             */
                            const uint64_t oti = ordinalTableIndex++;
                            ordinalTable[oti].symbolName = symbolName;
                            ordinalTable[oti].addend = addend;
                            ordinalTable[oti].libraryOrdinal
                            = libraryOrdinalSet ? libraryOrdinal : 0;
                            ordinalTable[oti].flags = flags;
                            ordinalTable[oti].type = type;
                        }
                    }
                    else
                    {
                        if(pass == 2){
                            char* segname = strdup(segName ? segName : "??");
                            char* sectname = strdup(sectName ? sectName : "??");
                            if (strlen(segname)>16)
                                segname[16] = 0;
                            if (strlen(sectname)>16)
                                sectname[16] = 0;
                            (*dbi)[n].segname = segname;
                            (*dbi)[n].sectname = sectname;
                            (*dbi)[n].address = segStartAddr + segOffset;
                            (*dbi)[n].bind_type = type;
                            (*dbi)[n].addend = addend;
                            (*dbi)[n].dylibname = fromDylib ? fromDylib : "??";
                            (*dbi)[n].symbolname = (symbolName ? symbolName :
                                                    "Symbol name not set");
                            (*dbi)[n].weak_import = *weak_import != '\0';
                            (*dbi)[n].pointer_value = 0;
                        }
                        n++;
                        segOffset += sizeof_pointer;
                    }
                    break;
                case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
                    /*
                     * consume: segIndex, segOffset, symbolName,
                     *          libraryOrdinalSet, ordinalTableIndex,
                     *          ordinalTableCount, ordinalTable, libraryOrdinal,
                     *          addend, flags, type, segName, segStartAddr,
                     *          fromDylib, weak_import
                     * produce: sectName, segOffset, dbi
                     */
                    opName = "BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB";
                    error = checkSegAndOffset(segIndex, segOffset, segs, nsegs,
                                              segs64, nsegs64, TRUE);
                    if (error) {
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                        segIndex = -1;
                        segName = NULL;
                        segStartAddr = 0;
                        segOffset = 0;
                        sectName = NULL;
                    }
                    else {
                        sectName = sectionName(segIndex, segStartAddr+segOffset,
                                               segs, nsegs, segs64, nsegs64);
                    }
                    if (NULL == symbolName) {
                        error = "missing preceding BIND_OPCODE_SET_SYMBOL_"
                        "TRAILING_FLAGS_IMM opcode";
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                    }
                    if (FALSE == libraryOrdinalSet){
                        error = "missing preceding BIND_OPCODE_SET_DYLIB_"
                        "ORDINAL_* opcode";
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                    }
                    if(2 == pass){
                        char* segname = strdup(segName ? segName : "??");
                        char* sectname = strdup(sectName ? sectName : "??");
                        if (strlen(segname)>16)
                            segname[16] = 0;
                        if (strlen(sectname)>16)
                            sectname[16] = 0;
                        (*dbi)[n].segname = segname;
                        (*dbi)[n].sectname = sectname;
                        (*dbi)[n].address = segStartAddr+segOffset;
                        (*dbi)[n].bind_type = type;
                        (*dbi)[n].addend = addend;
                        (*dbi)[n].dylibname = fromDylib ? fromDylib : "??";
                        (*dbi)[n].symbolname = (symbolName ? symbolName :
                                                "Symbol name not set");
                        (*dbi)[n].weak_import = *weak_import != '\0';
                        (*dbi)[n].pointer_value = 0;
                    }
                    n++;
                    segOffset += read_uleb128(&p, end, &error) + sizeof_pointer;
                    if (error) {
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                        segIndex = -1;
                        segName = NULL;
                        segStartAddr = 0;
                        segOffset = 0;
                        sectName = NULL;
                    }
                    /*
                     * Note, this is not really an error until the next bind
                     * but make so sense for a BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB
                     * to not be followed by another bind operation.
                     */
                    error = checkSegAndOffset(segIndex, segOffset, segs, nsegs,
                                              segs64, nsegs64, FALSE);
                    if (error) {
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                        segIndex = -1;
                        segName = NULL;
                        segStartAddr = 0;
                        segOffset = 0;
                        sectName = NULL;
                    }
                    break;
                case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
                    /*
                     * consume: segIndex, segOffset, symbolName,
                     *          libraryOrdinalSet, ordinalTableIndex,
                     *          ordinalTableCount, ordinalTable, libraryOrdinal,
                     *          addend, flags, type, segName, segStartAddr,
                     *          fromDylib, weak_import
                     * produce: sectName, segOffset, dbi
                     */
                    opName = "BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED";
                    error = checkSegAndOffset(segIndex, segOffset, segs, nsegs,
                                              segs64, nsegs64, TRUE);
                    if (error) {
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                        segIndex = -1;
                        segName = NULL;
                        segStartAddr = 0;
                        segOffset = 0;
                        sectName = NULL;
                    }
                    else {
                        sectName = sectionName(segIndex, segStartAddr+segOffset,
                                               segs, nsegs, segs64, nsegs64);
                    }
                    if (NULL == symbolName) {
                        error = "missing preceding BIND_OPCODE_SET_SYMBOL_"
                        "TRAILING_FLAGS_IMM opcode";
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                    }
                    if (FALSE == libraryOrdinalSet){
                        error = "missing preceding BIND_OPCODE_SET_DYLIB_"
                        "ORDINAL_* opcode";
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                    }
                    if(2 == pass){
                        char* segname = strdup(segName ? segName : "??");
                        char* sectname = strdup(sectName ? sectName : "??");
                        if (strlen(segname)>16)
                            segname[16] = 0;
                        if (strlen(sectname)>16)
                            sectname[16] = 0;
                        (*dbi)[n].segname = segname;
                        (*dbi)[n].sectname = sectname;
                        (*dbi)[n].address = segStartAddr+segOffset;
                        (*dbi)[n].bind_type = type;
                        (*dbi)[n].addend = addend;
                        (*dbi)[n].dylibname = fromDylib ? fromDylib : "??";
                        (*dbi)[n].symbolname = (symbolName ? symbolName :
                                                "Symbol name not set");
                        (*dbi)[n].weak_import = *weak_import != '\0';
                        (*dbi)[n].pointer_value = 0;
                    }
                    n++;
                    segOffset += immediate * sizeof_pointer + sizeof_pointer;
                    /*
                     * Note, this is not really an error until the next bind
                     * but make so sense for a BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB
                     * to not be followed by another bind operation.
                     */
                    error = checkSegAndOffset(segIndex, segOffset, segs, nsegs,
                                              segs64, nsegs64, FALSE);
                    if (error) {
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                        segIndex = -1;
                        segName = NULL;
                        segStartAddr = 0;
                        segOffset = 0;
                        sectName = NULL;
                    }
                    break;
                case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
                    /*
                     * consume: p, end, segIndex, segOffset,symbolName,
                     *          libraryOrdinalSet, fromDylib,
                     * produce: count, skip, sectName
                     */
                    opName = "BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB";
                    count = (uint32_t)read_uleb128(&p, end, &error);
                    if (error) {
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                        count = 0;
                    }
                    skip = (uint32_t)read_uleb128(&p, end, &error);
                    if (error) {
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                        skip = 0;
                    }
                    error = checkSegAndOffset(segIndex, segOffset, segs, nsegs,
                                              segs64, nsegs64, TRUE);
                    if (error) {
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                        segIndex = -1;
                        segName = NULL;
                        segStartAddr = 0;
                        segOffset = 0;
                        sectName = NULL;
                    }
                    else {
                        sectName = sectionName(segIndex, segStartAddr + segOffset,
                                               segs, nsegs, segs64, nsegs64);
                    }
                    if (NULL == symbolName) {
                        error = "missing preceding BIND_OPCODE_SET_SYMBOL_"
                        "TRAILING_FLAGS_IMM opcode";
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                    }
                    if (FALSE == libraryOrdinalSet){
                        error = "missing preceding BIND_OPCODE_SET_DYLIB_"
                        "ORDINAL_* opcode";
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                    }
                    error = checkCountAndSkip(&count, skip, segIndex,
                                              segOffset, segs, nsegs,
                                              segs64, nsegs64);
                    if (error) {
                        printErrorBind(pass, print_errors, opName,
                                       opcode_start-start, error, &errorCount);
                        segIndex = -1;
                        segName = NULL;
                        segStartAddr = 0;
                        segOffset = 0;
                        sectName = NULL;
                    }
                    for (uint32_t i=0; i < count; ++i) {
                        if(2 == pass){
                            char* segname = strdup(segName ? segName : "??");
                            char* sectname = strdup(sectName ? sectName : "??");
                            if (strlen(segname)>16)
                                segname[16] = 0;
                            if (strlen(sectname)>16)
                                sectname[16] = 0;
                            (*dbi)[n].segname = segname;
                            (*dbi)[n].sectname = sectname;
                            (*dbi)[n].address = segStartAddr+segOffset;
                            (*dbi)[n].bind_type = type;
                            (*dbi)[n].addend = addend;
                            (*dbi)[n].dylibname = fromDylib ? fromDylib : "??";
                            (*dbi)[n].symbolname = (symbolName ? symbolName :
                                                    "Symbol name not set");
                            (*dbi)[n].weak_import = *weak_import != '\0';
                            (*dbi)[n].pointer_value = 0;
                        }
                        n++;
                        if (-1 != segIndex)
                            segOffset += skip + sizeof_pointer;
                    }
                    break;
                case BIND_OPCODE_THREADED:
                    /* Note the immediate is a sub opcode */
                    /*
                     * [MDT: Giving up on 80 column formatting here... ]
                     */
                    switch(immediate){
                        case BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB:
                            /*
                             * consume: p, end
                             * produce: ordinalTableCount, ordinalTableIndex,
                             *          ordinalTable, chain_format
                             */
                            opName = "BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB";
                            ordinalTableCount = read_uleb128(&p, end, &error);
                            if (error) {
                                printErrorBind(pass, print_errors, opName, opcode_start - start, error, &errorCount);
                                ordinalTableCount = 0;
                                ordinalTableIndex = (uint64_t)-1;
                            }
                            else {
                                ordinalTableIndex = 0;
                            }
                            ordinalTable = reallocate(ordinalTable, sizeof(struct ThreadedBindData) * ordinalTableCount);
                            *chain_format = CHAIN_FORMAT_ARM64E;
                            break;
                        case BIND_SUBOPCODE_THREADED_APPLY:
                            /*
                             * consume: ordinalTableIndex, ordinalTableCount,
                             *          ordinalTable, segIndex, segOffset,
                             *          segAddr,
                             * produce: sectName, offset, pointerAddress,
                             *          pointerPageStart, pointerLocation,
                             *          delta, ordinal,
                             */
                            opName = "BIND_SUBOPCODE_THREADED_APPLY";
                            /*
                             * At this point ordinalTableIndex should equal
                             * ordinalTableCount or we have a mismatch between
                             * BIND_OPCODE_DO_BIND and ordinalTableCount.
                             */
                            if(ordinalTableIndex != ordinalTableCount){
                                error = ("incorrect ordinal table size: count of previous "
                                         "BIND_OPCODE_DO_BIND opcodes don't match count in previous "
                                         "BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB opcodes");
                                printErrorBind(pass, print_errors, opName, opcode_start - start, error, &errorCount);
                            }
                            /*
                             * We check for segOffset + 8 as we need to read a
                             * 64-bit pointer.
                             */
                            error = checkSegAndOffset(segIndex, segOffset, segs, nsegs, segs64, nsegs64, FALSE);
                            if (error) {
                                printErrorBind(pass, print_errors, opName, opcode_start-start, error, &errorCount);
                                segIndex = -1;
                                segName = NULL;
                                segStartAddr = 0;
                                segOffset = 0;
                                sectName = NULL;
                            }
                            else {
                                sectName = sectionName(segIndex, segStartAddr + segOffset, segs, nsegs, segs64, nsegs64);
                            }
                            /*
                             * Check segStartAddr + segOffset is 8-byte aligned.
                             */
                            if(((segStartAddr + segOffset) & 0x3) != 0){
                                error = "bad segOffset, not 8-byte aligned";
                                printErrorBind(pass, print_errors, opName, opcode_start - start, error, &errorCount);
                            }
                            /*
                             * This is a start a new thread of Rebase/Bind
                             * pointer chain from the previously set segIndex
                             * and segOffset. Record these locations even if
                             * misaligned. Do not record these locations if
                             * segIndex is invalid.
                             */
                            if (-1 != segIndex) {
                                offset = segs64[segIndex]->fileoff + segOffset;
                                pointerAddress = segs64[segIndex]->vmaddr + segOffset;
                                pointerPageStart = pointerAddress & ~0x3fff;
                                delta = 0;
                                do{
                                    uint64_t value;
                                    enum bool isRebase;
                                    pointerLocation = object_addr + offset;
                                    value = *(uint64_t *)pointerLocation;
                                    if(swapped)
                                        value = SWAP_LONG_LONG(value);
                                    isRebase = (value & (1ULL << 62)) == 0;
                                    if(isRebase){
                                        /* not doing anything with Rebase,
                                         only bind so no code here. */
                                        ;
                                    } else {
                                        /* the ordinal is bits are [0..15] */
                                        ordinal = value & 0xFFFF;
                                        if(ordinal >= ordinalTableCount){
                                            printErrorOrdAddr(pass, print_errors, opName, opcode_start - start,
                                                              ordinal, pointerAddress, &errorCount);
                                            break;
                                        }
                                        libraryOrdinal = ordinalTable[ordinal].libraryOrdinal;
                                        if (!validateOrdinal(libraryOrdinal, dylibs, ndylibs, &fromDylib, &error)) {
                                            printErrorOrdinal(pass, print_errors, opName, opcode_start - start, libraryOrdinal, ndylibs, error, &errorCount);
                                            break;
                                        }
                                        flags = ordinalTable[ordinal].flags;
                                        if((flags & BIND_SYMBOL_FLAGS_WEAK_IMPORT) != 0)
                                            weak_import = " (weak import)";
                                        else
                                            weak_import = "";
                                        if(pass == 2){
                                            enum bool has_auth = FALSE;
                                            uint16_t auth_diversity = 0;
                                            enum bool auth_addr_div = FALSE;
                                            uint8_t auth_key = 0;
                                            char* segname = strdup(segName ? segName : "??");
                                            char* sectname = strdup(sectName ? sectName : "??");

                                            if (strlen(segname)>16)
                                                segname[16] = 0;
                                            if (strlen(sectname)>16)
                                                sectname[16] = 0;
                                            if (value & ((uint64_t)1 << 63)) {
                                                has_auth = TRUE;
                                                auth_diversity = (value >> 32) & 0xFFFF;
                                                auth_addr_div = (value >> 48) & 1;
                                                auth_key = (value >> 49) & 2;
                                            }

                                            (*dbi)[n].segname = segname;
                                            (*dbi)[n].sectname = sectname;
                                            (*dbi)[n].address = segStartAddr+segOffset;
                                            (*dbi)[n].bind_type = ordinalTable[ordinal].type;
                                            (*dbi)[n].addend = ordinalTable[ordinal].addend;
                                            (*dbi)[n].dylibname = fromDylib;
                                            (*dbi)[n].symbolname = ordinalTable[ordinal].symbolName;
                                            (*dbi)[n].weak_import = *weak_import != '\0';
                                            (*dbi)[n].pointer_value = value;
                                            (*dbi)[n].pointer_ondisk = value;
                                            (*dbi)[n].pointer_format = CHAIN_FORMAT_ARM64E;
                                            if (has_auth) {
                                                (*dbi)[n].has_auth = has_auth;
                                                (*dbi)[n].auth_diversity = auth_diversity;
                                                (*dbi)[n].auth_addr_div = auth_addr_div;
                                                (*dbi)[n].auth_key = auth_key;
                                            }
                                        }
                                        n++;
                                    }
                                    
                                    /*
                                     * Now on to the next pointer in the chain if there
                                     * is one.
                                     */
                                    /* The delta is bits [51..61] */
                                    /* And bit 62 is to tell us if we are a rebase (0)
                                     or bind (1) */
                                    value &= ~(1ULL << 62);
                                    delta = (value & 0x3FF8000000000000) >> 51;
                                    /*
                                     * If the delta is zero there is no next pointer so
                                     * don't check the offset to the next pointer.
                                     */
                                    if(delta == 0)
                                        break;
                                    segOffset += delta * 8; /* sizeof(pint_t); */
                                    /*
                                     * Want to check that the segOffset plus 8 is not
                                     * past the end of this file and on the same page
                                     * in this segment so we can get the next pointer
                                     * in this thread.
                                     */
                                    offset = segs64[segIndex]->fileoff + segOffset;
                                    pointerAddress = segs64[segIndex]->vmaddr +
                                    segOffset;
                                    if(offset + 8 > object_size){
                                        printErrorOffset(pass, print_errors, opName, opcode_start - start,
                                                         pointerAddress, 0, &errorCount);
                                        break;
                                    }
                                    if(pointerPageStart != (pointerAddress & ~0x3fff)){
                                        printErrorOffset(pass, print_errors, opName, opcode_start - start,
                                                         pointerAddress, 1, &errorCount);
                                        break;
                                    }
                                } while(delta != 0);
                            }
                            break;
                        default:
                            printErrorOpcode(pass, print_errors, opName,
                                             opcode_start - start, immediate,
                                             &errorCount);
                            done = TRUE;
                            break;
                    }
                    break;
                default:
                    printErrorOpcode(pass, print_errors, NULL,
                                     opcode_start - start, opcode, &errorCount);
                    done = TRUE;
                    break;
            }
        }
    }
    if(ordinalTable != NULL)
        free(ordinalTable);
}

/*
 * cmp_dbi_ptr() compares two struct dyld_bind_info** values by address.
 */
static
int
cmp_dbi_ptr(
const void* va,
const void* vb)
{
    struct dyld_bind_info** a = (struct dyld_bind_info**)va;
    struct dyld_bind_info** b = (struct dyld_bind_info**)vb;
    if ((*a)->address < (*b)->address)
        return -1;
    if ((*a)->address > (*b)->address)
        return 1;
    return 0;
}

/*
 * get_dyld_bind_info_index() returns an array of pointers into the supplied
 * dbi array sorted by dyld_bind_info.address. This can be used to speed up
 * symbol lookup in get_dyld_bind_info_symbolname().
 */
extern
struct dyld_bind_info **
get_dyld_bind_info_index(
struct dyld_bind_info *dbi,
uint64_t ndbi)
{
    struct dyld_bind_info **dbi_index;

    dbi_index = calloc(ndbi, sizeof(*dbi_index));
    for (int i = 0; i < ndbi; ++i) {
        dbi_index[i] = &dbi[i];
    }
    qsort(dbi_index, ndbi, sizeof(*dbi_index), cmp_dbi_ptr);

    return dbi_index;
}

/*
 * print_dyld_rebase_opcodes() prints the raw contents of the DYLD_INFO rebase
 * data. Its purpose is to "disassemble" the contents of the opcode stream,
 * not to interpret the data. It is resilient to errors in the data, making it
 * useful for debugging malformed binaries.
 *
 * opcodes are printed in a concise table of data:
 *
 *   index, file offset, raw byte, symbolic opcode and opcode-specific data
 *
 * this format differs from dyldinfo(1) slightly, in that the offset is
 * relative to the start of mach_header, rather than to the start of the
 * opcode stream. Meaning, this output can be compared directly with commands
 * such as hexdump(1).
 */
extern
void
print_dyld_rebase_opcodes(
const uint8_t* object_addr,
uint64_t object_size,
uint32_t offset,
uint32_t size)
{
    if (offset > object_size) {
        printf("rebase offset %u beyond the size of file %llu\n",
               offset, object_size);
        return;
    }
    if (offset + size > object_size) {
        printf("rebase offset + size %u beyond the size of file %llu\n",
               offset + size, object_size);
        size = (uint32_t)(offset - object_size);
    }

    const uint8_t* p = object_addr + offset;
    const uint8_t* end = object_addr + offset + size;
    enum bool done = FALSE;
    uint32_t idx = 0;

    printf("rebase opcodes:\n");
    printf("  %4s %-10s %4s %s\n", "idx", "offset", "byte", "opcode");

    while (!done)
    {
        uint32_t poffset = (uint32_t)(p - object_addr);
        idx++;

        if (poffset >= object_size) {
            printf("rebase opcode #%u offset %u beyond the size of file %llu\n",
                   idx, poffset, object_size);
            break;
        }
        if (p >= end) {
            printf("%s opcode #%u offset %u beyond the size of opcodes %u\n",
                   "rebase", idx, poffset, size);
            break;
        }

        uint8_t byte = *p;
        uint8_t immediate = byte & REBASE_IMMEDIATE_MASK;
        uint8_t opcode = byte & REBASE_OPCODE_MASK;
        ++p;

        const char* name = NULL;
        const char* error = NULL;
        uint64_t uleb = 0;

        switch (opcode) {
                /*
                 * codes with IMM
                 */
            case REBASE_OPCODE_SET_TYPE_IMM:
                if (!name)
                    name = "REBASE_OPCODE_SET_TYPE_IMM";
                /* FALLTHROUGH */
            case REBASE_OPCODE_ADD_ADDR_IMM_SCALED:
                if (!name)
                    name = "REBASE_OPCODE_ADD_ADDR_IMM_SCALED";
                /* FALLTHROUGH */
            case REBASE_OPCODE_DO_REBASE_IMM_TIMES:
                if (!name)
                    name = "REBASE_OPCODE_DO_REBASE_IMM_TIMES";
                printf("  %4u 0x%08x 0x%02x %s(%d)\n",
                       idx, poffset, byte, name, immediate);
                break;
                /*
                 * codes with ULEB
                 */
            case REBASE_OPCODE_ADD_ADDR_ULEB:
                if (!name)
                    name = "REBASE_OPCODE_ADD_ADDR_ULEB";
                /* FALLTHROUGH */
            case REBASE_OPCODE_DO_REBASE_ULEB_TIMES:
                if (!name)
                    name = "REBASE_OPCODE_DO_REBASE_ULEB_TIMES";
                /* FALLTHROUGH */
            case REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
                if (!name)
                    name = "REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB";
                uleb = read_uleb128(&p, end, &error);
                if (error)
                    printf("  %4u 0x%08x 0x%02x %s(error: %s)\n",
                           idx, poffset, byte, name, error);
                else
                    printf("  %4u 0x%08x 0x%02x %s(0x%llx)\n",
                           idx, poffset, byte, name, uleb);
                break;
                /*
                 * codes with IMM and ULEB
                 */
            case REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
                name = "REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB";
                uleb = read_uleb128(&p, end, &error);
                if (error)
                    printf("  %4u 0x%08x 0x%02x %s(%d, error: %s)\n",
                           idx, poffset, byte, name, immediate, error);
                else
                    printf("  %4u 0x%08x 0x%02x %s(%d, 0x%llx)\n",
                           idx, poffset, byte, name, immediate, uleb);
                break;
                /*
                 * codes with ULEB and ULEB
                 */
            case REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
                name = "REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB";
                printf("  %4u 0x%08x 0x%02x %s(", idx, poffset, byte, name);
                uleb = read_uleb128(&p, end, &error);
                if (error)
                    printf("error: %s, ", error);
                else
                    printf("0x%llx, ", uleb);
                uleb = read_uleb128(&p, end, &error);
                if (error)
                    printf("error: %s)\n", error);
                else
                    printf("0x%llx)\n", uleb);
                break;
                /*
                 * codes with no arguments
                 */
            case REBASE_OPCODE_DONE:
                name = "REBASE_OPCODE_DONE";
                done = TRUE;
                /* FALLTHROUGH */
            default:
                if (!name)
                    name = "UNKNOWN OPCODE";
                printf("  %4u 0x%08x 0x%02x %s\n", idx, poffset, byte, name);
        }
    }
}

/*
 * print_dyld_bind_opcodes() prints the raw contents of DYLD_INFO bind data.
 * Its purpose is to "disassemble" the contents of the opcode stream, not to
 * interpret the data. It is resilient to errors in the data, making it useful
 * for debugging malformed binaries.
 *
 * opcodes are printed in a concise table of data:
 *
 *   index, file offset, raw byte, symbolic opcode and opcode-specific data
 *
 * this format differs from dyldinfo(1) slightly, in that the offset is
 * relative to the start of mach_header, rather than to the start of the
 * opcode stream. Meaning, this output can be compared directly with commands
 * such as hexdump(1).
 */
extern
void
print_dyld_bind_opcodes(
const uint8_t* object_addr,
uint64_t object_size,
const char* type,
uint32_t offset,
uint32_t size)
{
    if (offset > object_size) {
        printf("%s offset %u beyond the size of file %llu\n",
               type, offset, object_size);
        return;
    }
    if (offset + size > object_size) {
        printf("%s offset + size %u beyond the size of file %llu\n",
               type, offset + size, object_size);
        size = (uint32_t)(offset - object_size);
    }

    const uint8_t* p = object_addr + offset;
    const uint8_t* end = object_addr + offset + size;
    uint32_t idx = 0;

    printf("%s opcodes:\n", type);
    printf("  %4s %-10s %4s %s\n", "idx", "offset", "byte", "opcode");

    while (p != end)
    {
        uint32_t poffset = (uint32_t)(p - object_addr);
        idx++;

        uint8_t byte = *p;
        uint8_t immediate = byte & BIND_IMMEDIATE_MASK;
        uint8_t opcode = byte & BIND_OPCODE_MASK;
        ++p;

        const char* name = NULL;
        const char* error = NULL;
        const char* symbol = NULL;
        uint64_t uleb = 0;
        int64_t sleb = 0;

        switch (opcode) {
                /*
                 * codes with IMM
                 */
            case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
                if (!name)
                    name = "BIND_OPCODE_SET_DYLIB_ORDINAL_IMM";
                /* FALLTHROUGH */
            case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
                if (!name)
                    name = "BIND_OPCODE_SET_DYLIB_SPECIAL_IMM";
                /* FALLTHROUGH */
            case BIND_OPCODE_SET_TYPE_IMM:
                if (!name)
                    name = "BIND_OPCODE_SET_TYPE_IMM";
                /* FALLTHROUGH */
            case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
                if (!name)
                    name = "BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED";
                printf("  %4u 0x%08x 0x%02x %s(%d)\n",
                       idx, poffset, byte, name, immediate);
                break;
                /*
                 * codes with IMM and symbol name
                 */
            case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
                name = "BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM";
                printf("  %4u 0x%08x 0x%02x %s(%d, ",
                       idx, poffset, byte, name, immediate);
                symbol = (const char*)p;
                while(p < end && *p != '\0')
                    ++p;
                if (p < end && *p == '\0') {
                    if (p == (uint8_t*)symbol)
                        printf("(empty string))\n");
                    else
                        printf("%s)\n", symbol);
                    ++p;
                }
                else {
                    size_t len = strlen(symbol);
                    char* str = malloc(len + 1);
                    if (str) {
                        memcpy(str, symbol, len);
                        str[len] = '\0';
                        printf("%s (unterminated string)", str);
                        free(str);
                    }
                    printf(")\n");
                }
                break;
                /*
                 * codes with SLEB
                 */
            case BIND_OPCODE_SET_ADDEND_SLEB:
                name = "BIND_OPCODE_SET_ADDEND_SLEB";
                sleb = read_sleb128(&p, end, &error);
                if (error)
                    printf("  %4u 0x%08x 0x%02x %s(error: %s)\n",
                           idx, poffset, byte, name, error);
                else
                    printf("  %4u 0x%08x 0x%02x %s(%lld)\n",
                           idx, poffset, byte, name, sleb);
                break;
                /*
                 * codes with ULEB
                 */
            case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
                if (!name)
                    name = "BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB";
                /* FALLTHROUGH */
            case BIND_OPCODE_ADD_ADDR_ULEB:
                if (!name)
                    name = "BIND_OPCODE_ADD_ADDR_ULEB";
                /* FALLTHROUGH */
            case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
                if (!name)
                    name = "BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB";
                uleb = read_uleb128(&p, end, &error);
                if (error)
                    printf("  %4u 0x%08x 0x%02x %s(error: %s)\n",
                           idx, poffset, byte, name, error);
                else
                    printf("  %4u 0x%08x 0x%02x %s(0x%llx)\n",
                           idx, poffset, byte, name, uleb);
                break;
                /*
                 * codes with IMM and ULEB
                 */
            case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
                name = "BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB";
                uleb = read_uleb128(&p, end, &error);
                if (error)
                    printf("  %4u 0x%08x 0x%02x %s(%d, error: %s)\n",
                           idx, poffset, byte, name, immediate, error);
                else
                    printf("  %4u 0x%08x 0x%02x %s(%d, 0x%llx)\n",
                           idx, poffset, byte, name, immediate, uleb);
                break;
                /*
                 * codes with ULEB and ULEB
                 */
            case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
                name = "BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB";
                printf("  %4u 0x%08x 0x%02x %s(", idx, poffset, byte, name);
                uleb = read_uleb128(&p, end, &error);
                if (error)
                    printf("error: %s, ", error);
                else
                    printf("0x%llx, ", uleb);
                uleb = read_uleb128(&p, end, &error);
                if (error)
                    printf("error: %s)\n", error);
                else
                    printf("0x%llx)\n", uleb);
                break;
                /*
                 * codes with IMM sub-opcodes
                 */
            case BIND_OPCODE_THREADED:
                switch (immediate) {
                        /*
                         * codes with ULEB
                         */
                    case BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB:
                        name = ("BIND_OPCODE_THREADED "
                                "BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_"
                                "TABLE_SIZE_ULEB");
                        uleb = read_uleb128(&p, end, &error);
                        if (error)
                            printf("  %4u 0x%08x 0x%02x %s(error: %s)\n",
                                   idx, poffset, byte, name, error);
                        else
                            printf("  %4u 0x%08x 0x%02x %s(0x%llx)\n",
                                   idx, poffset, byte, name, uleb);
                        break;
                        /*
                         * codes with no arguments
                         */
                    case BIND_SUBOPCODE_THREADED_APPLY:
                        name = ("BIND_OPCODE_THREADED "
                                "BIND_SUBOPCODE_THREADED_APPLY");
                    default:
                        if (!name)
                            name = ("BIND_OPCODE_THREADED "
                                    "UNKNOWN SUBOPCODE");
                        printf("  %4u 0x%08x 0x%02x %s\n",
                               idx, poffset, byte, name);
                        break;
                }
                break;
                /*
                 * codes with no arguments
                 */
            case BIND_OPCODE_DONE:
                name = "BIND_OPCODE_DONE";
                /* FALLTHROUGH */
            case BIND_OPCODE_DO_BIND:
                if (!name)
                    name = "BIND_OPCODE_DO_BIND";
                /* FALLTHROUGH */
            default:
                if (!name)
                    name = "UNKNOWN OPCODE";
                printf("  %4u 0x%08x 0x%02x %s\n", idx, poffset, byte, name);
        }
    }
}

/*
 * print_dyld_bind_info() prints the internal expanded dyld bind information in
 * the same format as dyldinfo(1)'s -bind option.
 */
void
print_dyld_bind_info(
struct dyld_bind_info *dbi,
uint64_t ndbi)
{
    uint64_t n;
    static const char *keyNames[] = { "IA", "IB", "DA", "DB" };
    enum bool hasPointerFormat = FALSE;
    enum bool hasAuth = FALSE;

    if (0 == ndbi)
        return;
    
    enum bool is32 = (DYLD_CHAINED_PTR_32 == dbi[0].pointer_format ||
                      DYLD_CHAINED_PTR_32_CACHE == dbi[0].pointer_format ||
                      DYLD_CHAINED_PTR_32_FIRMWARE == dbi[0].pointer_format);
    
    size_t maxsegname = strlen("segment");
    size_t maxsectname = strlen("section");
    size_t maxdylibname = strlen("dylib");
    size_t maxaddr = 0;
    size_t maxvalue = is32 ? 10 : 18;
    
    for (n = 0; n < ndbi; n++) {
        const char* name = dbi[n].segname ? dbi[n].segname : "";
        size_t len = strlen(name);
        if (len > maxsegname)
            maxsegname = len;
        name = dbi[n].sectname ? dbi[n].sectname : "";
        len = strlen(name);
        if (len > maxsectname)
            maxsectname = len;
        name = dbi[n].dylibname ? dbi[n].dylibname : "";
        len = strlen(name);
        if (len > maxdylibname)
            maxdylibname = len;
        len = snprintf(NULL, 0, "%llX", dbi[n].address);
        if (len > maxaddr)
            maxaddr = len;
        if (dbi[n].pointer_format)
            hasPointerFormat = TRUE;
        if (dbi[n].has_auth)
            hasAuth = TRUE;
    }

    /* maxaddr needs to be wide enough to hold "address" */
    if (maxaddr < 5)
        maxaddr = 5;

    /*
     * Chained Fixups and Threaded Rebase use a format that prints the on-disk
     * pointer value. Non-threaded dyld info opcodes will use the historical
     * format. Over time, Mach-O binaries will migrate towards Chained Fixups.
     */
    if (hasPointerFormat) {
        printf("dyld information:\n");
        printf("%*s %*s %*s %*s %-6s",
               -(int)maxsegname, "segment",
               -(int)maxsectname, "section",
               -(int)(maxaddr+2), "address",
               -(int)maxvalue, "pointer",
               "type");
        if (hasAuth)
            printf(" div    addr  key");
        printf(" %-8s %*s %s\n",
               "addend",
               -(int)maxdylibname, "dylib",
               "symbol/vm address");
    } else {
        printf("bind information:\n");
        printf("%*s %*s %*s %*s %-11s %-8s %*s %s\n",
               -(int)maxsegname, "segment",
               -(int)maxsectname, "section",
               -(int)(maxaddr+2), "address",
               -(int)maxvalue, "value",
               "type",
               "addend",
               -(int)maxdylibname, "dylib",
               "symbol");
    }
    for(n = 0; n < ndbi; n++){
        uint64_t value;
        uint64_t target;
        const char* bind_name;
        const char* symbolName;
        const char* dylibName;
        char valuestr[20];
        enum bool isAuth;
        enum bool addrDiversity;
        uint16_t diversity;
        uint8_t key;

        bind_name = (dbi[n].bind_name ? dbi[n].bind_name :
                     bindTypeName(dbi[n].bind_type));
        dylibName = dbi[n].dylibname ? dbi[n].dylibname : "";
        symbolName = dbi[n].symbolname ? dbi[n].symbolname : NULL;
        value = hasPointerFormat ? dbi[n].pointer_ondisk : dbi[n].pointer_value;
        target = hasPointerFormat ? dbi[n].pointer_value : 0;
        isAuth = dbi[n].has_auth;
        diversity = isAuth ? dbi[n].auth_diversity : 0;
        addrDiversity = isAuth ? dbi[n].auth_addr_div : 0;
        key = isAuth ? dbi[n].auth_key : 0;

        if (value) {
            const char* format;
            format = is32 ? "0x%08llX" : "0x%016llX";
            snprintf(valuestr, sizeof(valuestr), format, value);
        }
        else {
            snprintf(valuestr, sizeof(valuestr), "");
        }

        if (hasPointerFormat) {
            printf("%*s %*s 0x%*llX %*s %-6s",
                   -(int)maxsegname, dbi[n].segname,
                   -(int)maxsectname, dbi[n].sectname,
                   -(int)maxaddr, dbi[n].address,
                   -(int)maxvalue, valuestr,
                   bind_name);
            if (hasAuth && isAuth) {
                const char* keyName = NULL;
                if (key < (sizeof(keyNames) / sizeof(*keyNames)))
                    keyName = keyNames[key];
                printf(" 0x%04X %-5s %-3s",
                       diversity,
                       addrDiversity ? "true" : "false",
                       keyName);
            } else if (hasAuth) {
                printf(" %16s", "");
            }
            if (dbi[n].bind_type)
                printf(" 0x%-6llX", dbi[n].addend);
            else
                printf(" %8s", "");
            printf(" %*s", -(int)maxdylibname, dylibName);
            if (symbolName) {
                printf(" %s%s",
                       symbolName,
                       dbi[n].weak_import ? " (weak import)" : "");
            }
            else if (target) {
                printf(" 0x%llX", target);
            }
        }
        else {
            printf("%*s %*s 0x%*llX %*s %-11s 0x%-6llX %*s %s%s",
                   -(int)maxsegname, dbi[n].segname,
                   -(int)maxsectname, dbi[n].sectname,
                   -(int)maxaddr, dbi[n].address,
                   -(int)maxvalue, valuestr,
                   bind_name,
                   dbi[n].addend,
                   -(int)maxdylibname, dylibName,
                   symbolName ? symbolName : "",
                   dbi[n].weak_import ? " (weak import)" : "");
        }

        printf("\n");
    }
}

/*
 * get_dyld_bind_info_symbolname() is passed an address and the internal
 * expanded dyld bind information.  If the address is found its binding symbol
 * name is returned.  If not NULL.
 */
const char *
get_dyld_bind_info_symbolname(
uint64_t address,
struct dyld_bind_info *dbi,
uint64_t ndbi,
struct dyld_bind_info **dbi_index,
enum chain_format_t chain_format,
int64_t *addend)
{
    uint64_t n;
    struct dyld_bind_info *info;

    info = NULL;
    if (dbi_index) {
        struct dyld_bind_info _key = {0}, *key = &_key;
        struct dyld_bind_info **value;
        key->address = address;
        value = bsearch(&key, dbi_index, ndbi, sizeof(*dbi_index), cmp_dbi_ptr);
        if (value)
            info = *value;
    }
    else {
        for(n = 0; n < ndbi; n++){
            if(dbi[n].address == address){
                info = &dbi[n];
                break;
            }
        }
    }

    if (info) {
        if(chain_format && addend != NULL)
            *addend = info->addend;
        return info->symbolname;
    }

    return(NULL);
}

/*****************************************************************************
 *
 *   Chained Fixups
 *
 */

/*
 * cached_string() is a gross hack. Sometimes the otool driver creates temporary
 * segment tables when building the chained fixup bind/rebase information, and
 * then the tables down before reading that information. In order to postpone
 * untangling this problem, strings returned by the chained fixup code will
 * point into a singleton cache that cannot be freed. This should just be
 * limited to section and segment names, so this O(N) algorithm should be ok,
 * although I concede this whole situation lacks greatness.
 */
static
const char*
cached_string(const char* s)
{
    static const char** cstrs;
    static uint32_t ncstr;

    if (NULL == s)
        return s;

    for (uint32_t i = 0; i < ncstr; ++i)
        if (0 == strcmp(cstrs[i], s))
            return cstrs[i];

    cstrs = reallocf(cstrs, sizeof(*cstrs) * (ncstr+1));
    cstrs[ncstr] = strdup(s);
    return cstrs[ncstr++];
}

/*
 * swap16, swap32, and swap64 are little inline functions for abstracting away
 * endian swapping.
 */
static inline uint16_t swap16(uint16_t x, enum bool swapped)
{
    return swapped == TRUE ? OSSwapInt16(x): x;
}
static inline uint32_t swap32(uint32_t x, enum bool swapped)
{
    return swapped == TRUE ? OSSwapInt32(x): x;
}
static inline uint64_t swap64(uint64_t x, enum bool swapped)
{
    return swapped == TRUE ? OSSwapInt64(x): x;
}

/*
 * get_base_addr computes the base vmaddress from a list of segments. by
 * convention, this is the vmaddr of the __TEXT segment. It will return
 * (uint64_t)-1 on error;
 */
static uint64_t
get_base_addr(struct segment_command **segs,
              uint32_t nsegs,
              struct segment_command_64 **segs64,
              uint32_t nsegs64)
{
    for (uint32_t i = 0; i < nsegs64; ++i) {
        if (0 == strcmp("__TEXT", segs64[i]->segname))
            return segs64[i]->vmaddr;
    }
    for (uint32_t i = 0; i < nsegs; ++i) {
        if (0 == strcmp("__TEXT", segs[i]->segname))
            return segs[i]->vmaddr;
    }
    
    return ((uint64_t)-1);
}

/*
 * fixup_target comes from dyld3. it represents a fixup target, which is most
 * likely a symbol. this structure captures values from a variety of different
 * fixup formats.
 */
struct fixup_target
{
    uint64_t    value;
    const char* dylib;
    const char* symbolName;
    uint64_t    addend;
    enum bool   weakImport;
};

/*
 * signExtendedAddend64 comes from dyld3. It computes the sign extended
 * addend from a 64-bit pointer.
 */
static uint64_t signExtendedAddend64(uint64_t addend27)
{
    uint64_t top8Bits     = addend27 & 0x00007F80000ULL;
    uint64_t bottom19Bits = addend27 & 0x0000007FFFFULL;
    uint64_t newValue     = (top8Bits << 13) |
                            (((uint64_t)(bottom19Bits << 37) >> 37) &
                             0x00FFFFFFFFFFFFFF);
    return newValue;
}

/*
 * signExtendedAddend64e comes from dyld3. It computes the sign extended
 * addend from a 64-bit arm64e pointer (no auth).
 */
static uint64_t signExtendedAddend64e(uint64_t addend19)
{
    if ( addend19 & 0x40000 )
        return addend19 | 0xFFFFFFFFFFFC0000ULL;
    else
        return addend19;
}

/*
 * libOrdinalFromUInt8() returns a signed libOrdinal (aka dylibOrdinal) from
 * a uint8_t as found in the chained import data.
 */
static int libOrdinalFromUInt8(uint8_t value)
{
    if (((uint8_t)-1) == value)
        return -1;
    else if (((uint8_t)-2) == value)
        return -2;
    else if (((uint8_t)-3) == value)
        return -3;
    return value;
}

/*
 * libOrdinalFromUInt16() returns a signed libOrdinal (aka dylibOrdinal) from
 * a uint16_t as found in the chained import data.
 */
static int libOrdinalFromUInt16(uint16_t value)
{
    if (((uint16_t)-1) == value)
        return -1;
    else if (((uint16_t)-2) == value)
        return -2;
    else if (((uint16_t)-3) == value)
        return -3;
    return value;
}

/*
 * get_fixup_targets builds a list of targets from a dyld_chained_fixups_header
 * and a list of dylibs ordered by ordinal. targets and ntarget may be
 * uninitialized prior to call.
 *
 * This function will print errors as they are encountered, some of which
 * will halt processing early and some of which will not.
 */
static void get_fixup_targets(/* inputs */
                              uint32_t datasize,
                              struct dyld_chained_fixups_header* header,
                              enum bool swapped,
                              const char **dylibs,
                              uint32_t ndylibs,
                              /* outputs */
                              struct fixup_target **targets,
                              uint32_t *ntarget
                              )
{
    /* build the list of fixup targets, one per symbol */
    uint32_t imports_count = swap32(header->imports_count, swapped);
    uint32_t imports_format = swap32(header->imports_format, swapped);
    uint32_t symbols_offset = swap32(header->symbols_offset, swapped);
    uint32_t max_symbols_offset = datasize - symbols_offset;
    const char* symbolsPool = (char*)header + symbols_offset;
    const char* dylibName;
    
    *ntarget = imports_count;
    *targets = reallocf(*targets, sizeof(struct fixup_target) * imports_count);
    memset(*targets, 0, sizeof(struct fixup_target) * imports_count);
    
    if (imports_format < 1 || imports_format > 3) {
        printf("unknown chained-fixups import format: %d\n", imports_format);
        return;
    }
    
    for (uint32_t i=0; i < imports_count; ++i) {
        int lib_ordinal = 0;

        if (imports_format == DYLD_CHAINED_IMPORT) {
            /* get a swapped dyld_chained_import struct */
            uint32_t values[1];
            memcpy(values, ((uint8_t*)header + header->imports_offset +
                            i * sizeof(struct dyld_chained_import)),
                   sizeof(values));
            values[0] = swap32(values[0], swapped);
            struct dyld_chained_import imports;
            memcpy(&imports, &values, sizeof(imports));

            /* look for overflow */
            if (imports.name_offset > max_symbols_offset) {
                printf("chained-fixups import #%u symbol offset extends "
                       "past end: %u (max %u)\n", i, imports.name_offset,
                       max_symbols_offset);
                imports.name_offset = 0;
            }

            /* record the information */
            (*targets)[i].symbolName = &symbolsPool[imports.name_offset];
            (*targets)[i].weakImport = imports.weak_import;
            lib_ordinal = libOrdinalFromUInt8(imports.lib_ordinal);
        }
        else if (imports_format == DYLD_CHAINED_IMPORT_ADDEND) {
            /* get a swapped dyld_chained_import_addend struct */
            uint32_t values[2];
            memcpy(values, ((uint8_t*)header + header->imports_offset +
                            i * sizeof(struct dyld_chained_import_addend)),
                   sizeof(values));
            values[0] = swap32(values[0], swapped);
            values[1] = swap32(values[1], swapped);
            struct dyld_chained_import_addend imports;
            memcpy(&imports, &values, sizeof(imports));
            
            /* look for overflow */
            if (imports.name_offset > max_symbols_offset) {
                printf("chained-fixups import #%u symbol offset extends "
                       "past end: %u (max %u)\n", i, imports.name_offset,
                       max_symbols_offset);
                imports.name_offset = 0;
            }
            
            /* record the information */
            (*targets)[i].symbolName = &symbolsPool[imports.name_offset];
            (*targets)[i].weakImport = imports.weak_import;
            (*targets)[i].addend = imports.addend;
            lib_ordinal = libOrdinalFromUInt8(imports.lib_ordinal);
        }
        else if (imports_format == DYLD_CHAINED_IMPORT_ADDEND64) {
            /* get a swapped dyld_chained_import_addend64 struct */
            uint64_t values[2];
            memcpy(values, ((uint8_t*)header + header->imports_offset +
                            i * sizeof(struct dyld_chained_import_addend64)),
                   sizeof(values));
            values[0] = swap64(values[0], swapped);
            values[1] = swap64(values[1], swapped);
            struct dyld_chained_import_addend64 imports;
            memcpy(&imports, &values, sizeof(imports));
            
            /* look for overflow */
            if (imports.name_offset > max_symbols_offset) {
                printf("chained-fixups import #%u symbol offset extends "
                       "past end: %u (max %u)\n", i, imports.name_offset,
                       max_symbols_offset);
                imports.name_offset = 0;
            }
            
            /* record the information */
            (*targets)[i].symbolName = &symbolsPool[imports.name_offset];
            (*targets)[i].weakImport = imports.weak_import;
            (*targets)[i].addend = imports.addend;
            lib_ordinal = libOrdinalFromUInt16(imports.lib_ordinal);
        }
        
        if (!validateOrdinal(lib_ordinal, dylibs, ndylibs, &dylibName, NULL)) {
            if (lib_ordinal < 0)
                printf("chained-fixups import #%u bad special library ordinal: "
                       "%d\n", i, lib_ordinal);
            else
                printf("chained-fixups import #%u bad library ordinal: %d "
                       "(max %u)\n", i, lib_ordinal, ndylibs);
            continue;
        }
        
        (*targets)[i].dylib = dylibName;
    }
}

static
uint64_t
pointer_rebase_next(uint64_t pointer,
             uint16_t pointer_format)
{
    uint32_t pointer32 = (uint32_t)pointer;

    // Using (enum chain_format_t) to produce warnings on missing cases.
    switch ((enum chain_format_t)pointer_format) {
        case DYLD_CHAINED_PTR_ARM64E:
        case DYLD_CHAINED_PTR_ARM64E_KERNEL:
        case DYLD_CHAINED_PTR_ARM64E_FIRMWARE:
        case DYLD_CHAINED_PTR_ARM64E_USERLAND:
        case DYLD_CHAINED_PTR_ARM64E_USERLAND24:
        {
            struct dyld_chained_ptr_arm64e_rebase rebase;
            memcpy(&rebase, &pointer, sizeof(rebase));
            if (rebase.bind)
                return 0;
            return rebase.next;
        }
        case DYLD_CHAINED_PTR_64:
        case DYLD_CHAINED_PTR_64_OFFSET:
        {
            struct dyld_chained_ptr_64_rebase rebase;
            memcpy(&rebase, &pointer, sizeof(rebase));
            if (rebase.bind)
                return 0;
            return rebase.next;
        }
        case DYLD_CHAINED_PTR_32:
        {
            struct dyld_chained_ptr_32_rebase rebase;
            memcpy(&rebase, &pointer32, sizeof(rebase));
            if (rebase.bind)
                return 0;
            return rebase.next;
        }
        case DYLD_CHAINED_PTR_32_CACHE:
        {
            struct dyld_chained_ptr_32_cache_rebase rebase;
            memcpy(&rebase, &pointer32, sizeof(rebase));
            return rebase.next;
        }
        case DYLD_CHAINED_PTR_32_FIRMWARE:
        {
            struct dyld_chained_ptr_32_firmware_rebase rebase;
            memcpy(&rebase, &pointer32, sizeof(rebase));
            return rebase.next;
        }
        case DYLD_CHAINED_PTR_64_KERNEL_CACHE:
        case DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE:
        {
            struct dyld_chained_ptr_64_kernel_cache_rebase rebase;
            memcpy(&rebase, &pointer, sizeof(rebase));
            return rebase.next;
        }
        case CHAIN_FORMAT_NONE:
            break;
    }

    return -1;
}

/*
 * walk_chain() walks a chain of binds and rebases, starting at the pointer
 * location indicated by the dyld_chained_starts_in_segment struct, the
 * pageIndex, and the offsetInPage values. The chain will be followed until
 * it ends.
 *
 * For each pointer found in the chain, walk_chain will build a dyld_bind_info
 * structure using information found in the segment lists and the list of fixup
 * targets (e.g., symbols). The dyld_bind_info is not quiiiiiite what we want
 * for this: it does not properly support BIND types vs REBASE types. But the
 * format used by the original arm64e format and the new chained binds format
 * are so semantically equivalent, it does a reasonable job.
 *
 * The input parameter 'segInfo' is optional. For load-based chained fixups,
 * the segInfo comes directly from the chained fixup data. For section-based
 * chained fixups, 'segInfo' should be NULL. Because the pointer_format value
 * must be passed in separately for section-based files, walk_chain() will use
 * this parameter consistently, and not consult the segInfo structure again.
 *
 * Similarly, the paramters 'seg_index' and 'segname' can be supplied if both
 * values are available; otherwise both should be zeroed out.
 *
 * The output values 'dbi' and 'ndbi' must be initialized to NULL / 0 before
 * first calling walk_chain; values modified by walk_chain may be passed
 * back into walk_chain safely.
 */
static void walk_chain(/* inputs */
                       char *object_addr,
                       uint64_t object_size,
                       uint64_t baseaddr,
                       struct segment_command **segs,
                       uint32_t nsegs,
                       struct segment_command_64 **segs64,
                       uint32_t nsegs64,
                       struct fixup_target *targets,
                       uint32_t ntarget,
                       enum bool swapped,
                       struct dyld_chained_starts_in_segment* segInfo,
                       uint32_t seg_index,
                       const char* segname,
                       uint16_t pointer_format,
                       uint16_t pageIndex,
                       uint16_t chain_offset,
                       /* output */
                       struct dyld_bind_info **dbi,
                       uint64_t *ndbi)
{
    void* chain;
    uint64_t chain_addr;
    uint32_t max_valid_pointer = 0;

    /*
     * set chain, chain_addr, and max_valid_pointer from the segInfo if
     * available; otherwise, set it directly from the chain_offset.
     */
    if (segInfo) {
        uint64_t segment_vmoff = swap64(segInfo->segment_offset, swapped);
        uint64_t segment_fileoff = segmentFileOffset(seg_index, segs, nsegs,
                                                     segs64, nsegs64);
        uint16_t page_size = swap16(segInfo->page_size, swapped);
        uint64_t page_vmoff = segment_vmoff + pageIndex * page_size;
        uint64_t page_fileoff = segment_fileoff + pageIndex * page_size;
        uint8_t* pageContentStart = (uint8_t*)(object_addr + page_fileoff);
        chain = (pageContentStart+chain_offset);
        chain_addr = baseaddr + page_vmoff + chain_offset;
        max_valid_pointer = segInfo->max_valid_pointer;
    }
    else {
        chain = object_addr + chain_offset;
        chain_addr = baseaddr + chain_offset;
    }

    /* loop over all of the fixups in the chain */
    enum bool done = FALSE;
    while (!done) {
        uint64_t addend = 0;
        const char* symbolName = NULL;
        const char* dylibName = NULL;
        const char* bindName = NULL;
        const char* sectname = NULL;
        enum bool weakImport = FALSE;
        uint64_t address = chain_addr;
        uint64_t pointer_value = 0;
        uint64_t pointer_ondisk = 0;
        enum bool has_auth = FALSE;
        uint16_t auth_diversity = 0;
        enum bool auth_addr_div = FALSE;
        uint8_t auth_key = 0;
        int bind_type = 0;

        /*
         * because section-based fixup chains are not confined to their
         * segments, we must look up new segment information for each
         * chain value.*/
        const char* this_segname;
        if (segname) {
            this_segname = segname;
        }
        else {
            seg_index = segmentIndexFromAddr(chain_addr, segs, nsegs,
                                             segs64, nsegs64);
            if (seg_index != -1) {
                this_segname = segmentName(seg_index, segs, nsegs,
                                           segs64, nsegs64);
            }
        }
        sectname = sectionName(seg_index, chain_addr, segs, nsegs,
                               segs64, nsegs64);

        /* set the bind and rebase information from the chain value */
        switch (pointer_format) {
            case DYLD_CHAINED_PTR_ARM64E:
            case DYLD_CHAINED_PTR_ARM64E_KERNEL:
            case DYLD_CHAINED_PTR_ARM64E_FIRMWARE:
            case DYLD_CHAINED_PTR_ARM64E_USERLAND:
            case DYLD_CHAINED_PTR_ARM64E_USERLAND24:
            {
                uint32_t stride = 8;
                uint64_t chain_value = swap64(*(uint64_t*)chain, swapped);
                pointer_ondisk = chain_value;

                enum bool isauth = FALSE;
                enum bool isbind = FALSE;

                struct dyld_chained_ptr_arm64e_auth_rebase auth_rebase;
                memcpy(&auth_rebase, &chain_value, sizeof(auth_rebase));
                isauth = auth_rebase.auth;
                isbind = auth_rebase.bind;
                has_auth = isauth;
                bind_type = isbind;

                if (isauth && isbind) {
                    uint32_t ordinal;

                    bindName = "bind";

                    if (pointer_format == DYLD_CHAINED_PTR_ARM64E_USERLAND24) {
                        struct dyld_chained_ptr_arm64e_auth_bind24 bind;
                        memcpy(&bind, &chain_value, sizeof(bind));
                        ordinal = bind.ordinal;
                        auth_diversity = bind.diversity;
                        auth_addr_div = bind.addrDiv;
                        auth_key = bind.key;
                    }
                    else {
                        struct dyld_chained_ptr_arm64e_auth_bind bind;
                        memcpy(&bind, &chain_value, sizeof(bind));
                        ordinal = bind.ordinal;
                        auth_diversity = bind.diversity;
                        auth_addr_div = bind.addrDiv;
                        auth_key = bind.key;
                    }

                    if (ordinal < ntarget) {
                        struct fixup_target* target = &targets[ordinal];
                        addend = target->addend;
                        symbolName = target->symbolName;
                        dylibName = target->dylib;
                        weakImport = target->weakImport;
                    } else {
                        printErrorFixupBindOrdinal(chain_addr, ordinal,
                                                   ntarget, NULL);
                    }
                }
                else if (!isauth && isbind) {
                    uint32_t ordinal;

                    bindName = "bind";

                    if (pointer_format == DYLD_CHAINED_PTR_ARM64E_USERLAND24) {
                        struct dyld_chained_ptr_arm64e_bind24 bind;
                        memcpy(&bind, &chain_value, sizeof(bind));
                        ordinal = bind.ordinal;
                        addend = bind.addend;
                    }
                    else {
                        struct dyld_chained_ptr_arm64e_bind bind;
                        memcpy(&bind, &chain_value, sizeof(bind));
                        ordinal = bind.ordinal;
                        addend = bind.addend;
                    }

                    if (ordinal < ntarget) {
                        struct fixup_target* target = &targets[ordinal];
                        addend = target->addend + signExtendedAddend64e(addend);
                        symbolName = target->symbolName;
                        dylibName = target->dylib;
                        weakImport = target->weakImport;
                    } else {
                        printErrorFixupBindOrdinal(chain_addr, ordinal,
                                                   ntarget, NULL);
                    }
                }
                else if (isauth && !isbind) {
                    // auth rebase is a vmoffset from __TEXT vmaddr
                    bindName = "rebase";
                    pointer_value = auth_rebase.target;
                    auth_diversity = auth_rebase.diversity;
                    auth_addr_div = auth_rebase.addrDiv;
                    auth_key = auth_rebase.key;
                }
                else if (!isauth && !isbind) {
                    // the old threaded rebase format is a vmaddr, but the new
                    // arm64e rebase formats are both vmoffsets.
                    struct dyld_chained_ptr_arm64e_rebase rebase;
                    memcpy(&rebase, &chain_value, sizeof(rebase));

                    bindName = "rebase";

                    pointer_value = (((uint64_t)(rebase.high8) << 56) |
                                     rebase.target);
                    if (pointer_format == DYLD_CHAINED_PTR_ARM64E_USERLAND ||
                        pointer_format == DYLD_CHAINED_PTR_ARM64E_USERLAND24) {
                        pointer_value += baseaddr;
                    }
                }

                if (pointer_format == DYLD_CHAINED_PTR_ARM64E_KERNEL ||
                    pointer_format==DYLD_CHAINED_PTR_ARM64E_FIRMWARE) {
                    stride = 4;
                }

                if (auth_rebase.next == 0) {
                    done = TRUE;
                }
                else {
                    chain += auth_rebase.next * stride;
                    chain_addr += auth_rebase.next * stride;
                }
            }
                break;
            case DYLD_CHAINED_PTR_64:
            case DYLD_CHAINED_PTR_64_OFFSET:
            {
                uint64_t chain_value = swap64(*(uint64_t*)chain, swapped);
                pointer_ondisk = chain_value;

                struct dyld_chained_ptr_64_bind bind;
                memcpy(&bind, &chain_value, sizeof(bind));
                
                if (bind.bind) {
                    bindName = "bind";
                    bind_type = 1;

                    if (bind.ordinal < ntarget) {
                        struct fixup_target* target = &targets[bind.ordinal];
                        addend = (target->addend +
                                  signExtendedAddend64(bind.addend));
                        symbolName = target->symbolName;
                        dylibName = target->dylib;
                        weakImport = target->weakImport;
                    } else {
                        printErrorFixupBindOrdinal(chain_addr, bind.ordinal,
                                                   ntarget, NULL);
                    }
                }
                else {
                    // The old DYLD_CHAINED_PTR_64 target is vmaddr, but
                    // DYLD_CHAINED_PTR_64_OFFSET target is vmoffset.
                    struct dyld_chained_ptr_64_rebase rebase;
                    memcpy(&rebase, &chain_value, sizeof(rebase));

                    bindName = "rebase";
                    pointer_value = (((uint64_t)(rebase.high8) << 56) |
                                     rebase.target);
                    if (pointer_format == DYLD_CHAINED_PTR_64_OFFSET)
                        pointer_value += baseaddr;
                }
                if (bind.next == 0) {
                    done = TRUE;
                }
                else {
                    chain += bind.next * 4;
                    chain_addr += bind.next * 4;
                }
            }
                break;
            case DYLD_CHAINED_PTR_64_KERNEL_CACHE:
            case DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE:
            {
                // kernel cache rebase targets are vmaddr
                uint32_t stride = 4;
                uint64_t chain_value = swap64(*(uint64_t*)chain, swapped);
                pointer_ondisk = chain_value;

                struct dyld_chained_ptr_64_kernel_cache_rebase rebase;
                memcpy(&rebase, &chain_value, sizeof(rebase));

                bindName = "rebase";
                pointer_value = rebase.target;

                if (pointer_format == DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE) {
                    stride = 1;
                }

                if (rebase.next == 0) {
                    done = TRUE;
                }
                else {
                    chain += rebase.next * stride;
                    chain_addr += rebase.next * stride;
                }
            }
                break;
            case DYLD_CHAINED_PTR_32:
            case DYLD_CHAINED_PTR_32_CACHE:
            case DYLD_CHAINED_PTR_32_FIRMWARE:
            {
                uint32_t chain_value = swap32(*(uint32_t*)chain, swapped);
                uint32_t next = 0;
                uint32_t stride = 4;
                pointer_ondisk = chain_value;

                if (pointer_format == DYLD_CHAINED_PTR_32) {
                    struct dyld_chained_ptr_32_bind bind;
                    memcpy(&bind, &chain_value, sizeof(bind));

                    if (bind.bind) {
                        bindName = "bind";
                        bind_type = 1;

                        if (bind.ordinal < ntarget) {
                            struct fixup_target* target = &targets[bind.ordinal];
                            addend = target->addend + bind.addend;
                            symbolName = target->symbolName;
                            dylibName = target->dylib;
                            weakImport = target->weakImport;
                        } else {
                            printErrorFixupBindOrdinal(chain_addr, bind.ordinal,
                                                       ntarget, NULL);
                        }

                        pointer_value = 0;
                        next = bind.next;
                    }
                    else {
                        // 32-bit rebase targets are vmaddr
                        struct dyld_chained_ptr_32_rebase rebase;
                        memcpy(&rebase, &chain_value, sizeof(rebase));

                        bindName = "rebase";

                        pointer_value = rebase.target;
                        next = rebase.next;
                    }
                }
                else if (pointer_format == DYLD_CHAINED_PTR_32_CACHE) {
                    // 32-bit rebase targets are vmaddr
                    struct dyld_chained_ptr_32_cache_rebase rebase;
                    memcpy(&rebase, &chain_value, sizeof(rebase));
                    bindName = "rebase";
                    pointer_value = rebase.target;
                    next = rebase.next;
                }
                else if (pointer_format == DYLD_CHAINED_PTR_32_FIRMWARE) {
                    // 32-bit rebase targets are vmaddr
                    struct dyld_chained_ptr_32_firmware_rebase rebase;
                    memcpy(&rebase, &chain_value, sizeof(rebase));
                    bindName = "rebase";
                    pointer_value = rebase.target;
                    next = rebase.next;
                }

                if (next == 0) {
                    done = TRUE;
                }
                else {
                    chain += next * stride;
                    chain_addr += next * stride;

                    /* Skip over non-pointers */
                    chain_value = swap32(*(uint32_t*)chain, swapped);
                    next = (uint32_t)pointer_rebase_next(chain_value,
                                                         pointer_format);
                    while (max_valid_pointer && next > max_valid_pointer)
                    {
                        chain += next * stride;
                        chain_addr += next * stride;
                        chain_value = swap32(*(uint32_t*)chain, swapped);
                        next = (uint32_t)pointer_rebase_next(chain_value,
                                                             pointer_format);
                    }
                }
            }
                break;
            default:
                /* otool does not support printing these formats */
                printf("bad chained fixups: unknown pointer format %d\n",
                       pointer_format);
                done = TRUE;
                continue;
        }

        /* add our dbi record if we found all needed data */
        uint64_t dbii = *ndbi;
        *ndbi += 1;
        *dbi = reallocf(*dbi, sizeof(**dbi) * (*ndbi));
        memset(&(*dbi)[dbii], 0, sizeof(**dbi));
        (*dbi)[dbii].segname = cached_string(this_segname);
        (*dbi)[dbii].sectname = cached_string(sectname);
        (*dbi)[dbii].address = address;
        (*dbi)[dbii].bind_name = bindName;
        (*dbi)[dbii].bind_type = bind_type;
        (*dbi)[dbii].addend = addend;
        (*dbi)[dbii].dylibname = dylibName;
        (*dbi)[dbii].symbolname = symbolName;
        (*dbi)[dbii].weak_import = weakImport;
        (*dbi)[dbii].pointer_value = pointer_value;
        (*dbi)[dbii].pointer_ondisk = pointer_ondisk;
        (*dbi)[dbii].pointer_format = pointer_format;
        (*dbi)[dbii].has_auth = has_auth;
        if (has_auth) {
            (*dbi)[dbii].auth_diversity = auth_diversity;
            (*dbi)[dbii].auth_addr_div = auth_addr_div;
            (*dbi)[dbii].auth_key = auth_key;
        }
    }
}

static
void
get_dyld_chained_fixups_section(/* input */
                                const uint8_t* start,
                                const uint8_t* end,
                                uint64_t baseaddr,
                                const char **dylibs,
                                uint32_t ndylibs,
                                struct segment_command **segs,
                                uint32_t nsegs,
                                struct segment_command_64 **segs64,
                                uint32_t nsegs64,
                                enum bool swapped,
                                char *object_addr,
                                uint64_t object_size,
                                /* output */
                                struct dyld_bind_info **dbi,
                                uint64_t *ndbi,
                                enum chain_format_t *chain_format)
{
    const char* header_addr = (const char*)start;
    uint64_t header_size = end - start;

    /* get the chain starts header */
    if (sizeof(struct dyld_chained_starts_offsets) > header_size) {
        printf("bad chained fixups: chained starts extends past end of "
               "data.\n");
        return;
    }

    const char* p = header_addr;
    struct dyld_chained_starts_offsets header;
    uint32_t left, size;

    left = (uint32_t)(object_size - (p - object_addr));
    memset((char *)&header, '\0', sizeof(header));
    size = left < sizeof(header) ? left : sizeof(header);
    memcpy((char *)&header, p, size);
    if(swapped) {
        header.pointer_format = _OSSwapInt32(header.pointer_format);
        header.starts_count = _OSSwapInt32(header.starts_count);
    }

    *chain_format = header.pointer_format;

    /* walk the chains */
    const char* q = p + sizeof(header) - sizeof(header.chain_starts);
    for(int i = 0; i < header.starts_count; ++i, q += sizeof(uint32_t)) {
        if (q - header_addr >= header_size) {
            printf("bad chained fixups: chain_start[%d] begins past end "
                   "of data.\n", i);
            continue;
        }
        if (q + sizeof(uint32_t) - header_addr > header_size) {
            printf("bad chained fixups: chain_start[%d] extends past end "
                   "of data.\n", i);
            continue;
        }
        if (q - object_addr >= object_size) {
            printf("bad chained fixups: chain_start[%d] begins past "
                   "end of file.\n", i);
            continue;
        }
        if (q + sizeof(uint32_t) - object_addr >= object_size) {
            printf("bad chained fixups: chain_start[%d] extends past "
                   "end of file.\n", i);
            continue;
        }

        uint32_t start = 0;
        memcpy(&start, q, sizeof(start));
        if(swapped) {
            start = _OSSwapInt32(start);
        }

        /* compute the segment index and segment name for the chain start */
        uint32_t segIndex = segmentIndexFromAddr(start + baseaddr,
                                                 segs, nsegs,
                                                 segs64, nsegs64);
        if (segIndex == -1) {
            printf("bad chained fixups: no segment found for "
                   "chain_start[%d] at address 0x%x\n", i, start);
            continue;
        }

        uint16_t pointer_format = header.pointer_format;
        walk_chain(object_addr, object_size, baseaddr, segs, nsegs,
                   segs64, nsegs64, NULL, 0, swapped, NULL, segIndex,
                   NULL, pointer_format, 0, start, dbi, ndbi);
    }
}

static
void
get_dyld_chained_fixups_lcmd(/* input */
                             const uint8_t* start,
                             const uint8_t* end,
                             uint64_t baseaddr,
                             const char **dylibs,
                             uint32_t ndylibs,
                             struct segment_command **segs,
                             uint32_t nsegs,
                             struct segment_command_64 **segs64,
                             uint32_t nsegs64,
                             enum bool swapped,
                             char *object_addr,
                             uint64_t object_size,
                             /* output */
                             struct dyld_bind_info **dbi,
                             uint64_t *ndbi,
                             enum chain_format_t *chain_format)
{
    struct dyld_chained_fixups_header* header;
    struct dyld_chained_starts_in_image* imageStarts;
    struct fixup_target* targets = NULL;
    uint32_t ntarget = 0;
    uint64_t end_offset = end - start;

    /* get the chained fixups header */
    if (sizeof(struct dyld_chained_fixups_header) + start > end) {
        printf("bad chained fixups: header size %lu extends past end %llu\n",
               sizeof(struct dyld_chained_fixups_header), end_offset);
        return;
    }
    header = (struct dyld_chained_fixups_header*)start;

    /* get the fixup targets */
    get_fixup_targets((uint32_t)(end - start), header, swapped, dylibs, ndylibs,
                      &targets, &ntarget);

    /* get the chained starts */
    uint32_t starts_offset = swap32(header->starts_offset, swapped);
    if (starts_offset < sizeof(struct dyld_chained_fixups_header)) {
        printf("bad chained fixups: image starts offset %u overlaps with "
               "fixups header size %lu\n", starts_offset,
               sizeof(struct dyld_chained_fixups_header));
        return;
    }
    if (starts_offset +
        sizeof(struct dyld_chained_starts_in_image) > end_offset) {
        printf("bad chained fixups: image starts end %lu extends past "
               "end %llu\n", starts_offset +
               sizeof(struct dyld_chained_starts_in_image), end_offset);
        return;
    }
    imageStarts = (struct dyld_chained_starts_in_image*)(start + starts_offset);

    /* walk the image starts */
    uint32_t nseg = swap32(imageStarts->seg_count, swapped);
    for (uint32_t iseg = 0; iseg < nseg; ++iseg)
    {
        struct dyld_chained_starts_in_segment* segInfo;

        uint32_t seg_info_offset = swap32(imageStarts->seg_info_offset[iseg],
                                          swapped);

        /* seg_info_offsets are allowed to be 0 */
        if (0 == seg_info_offset)
            continue;

        /*
         * dyld does not strictly check that the dyld_chained_starts_in_segment
         * struct fits in the Mach-O. As long as page_count is addressable
         * and has a 0 value, the rest of the struct can be any random data.
         */
        if (starts_offset + seg_info_offset > end_offset) {
            printf("bad chained fixups: seg_info %i offset %u starts past "
                   "end %llu\n", iseg, starts_offset + seg_info_offset,
                   end_offset);
            continue;
        }
        if (starts_offset + seg_info_offset +
            sizeof(struct dyld_chained_starts_in_segment) > end_offset) {
            printf("bad chained fixups: seg_info %i end %lu extends past "
                   "end %llu\n", iseg, starts_offset + seg_info_offset +
                   sizeof(struct dyld_chained_starts_in_segment), end_offset);
        }

        segInfo = ((struct dyld_chained_starts_in_segment*)
                   (start + starts_offset + seg_info_offset));

        const char* segname = segmentName(iseg, segs, nsegs, segs64, nsegs64);
        uint16_t page_count = swap16(segInfo->page_count, swapped);
        for (uint16_t pageIndex = 0; pageIndex < page_count; ++pageIndex)
        {
            /* set the chain format. If already set, verify it. */
            uint16_t pointer_format = swap16(segInfo->pointer_format, swapped);
            if (CHAIN_FORMAT_NONE == *chain_format) {
                *chain_format = (enum chain_format_t)pointer_format;
            }
            else if (pointer_format != *chain_format) {
                printf("bad chained fixups: seg_info %i pointer format %d "
                       "does not match previous pointer format %d\n", iseg,
                       pointer_format, *chain_format);
            }

            /* walk the chains */
            uint16_t offsetInPage = swap16(segInfo->page_start[pageIndex],
                                           swapped);
            if ( offsetInPage == DYLD_CHAINED_PTR_START_NONE )
                continue;
            if ( offsetInPage & DYLD_CHAINED_PTR_START_MULTI ) {
                uint32_t overflowIndex =
                offsetInPage & ~DYLD_CHAINED_PTR_START_MULTI;
                enum bool chainEnd = FALSE;
                while (!chainEnd) {
                    uint16_t page_start =
                        swap16(segInfo->page_start[overflowIndex], swapped);
                    chainEnd = (page_start & DYLD_CHAINED_PTR_START_LAST);
                    offsetInPage = (page_start & ~DYLD_CHAINED_PTR_START_LAST);
                    walk_chain(object_addr, object_size, baseaddr, segs, nsegs,
                               segs64, nsegs64, targets, ntarget, swapped,
                               segInfo, iseg, segname, pointer_format,
                               pageIndex, offsetInPage, dbi, ndbi);
                    ++overflowIndex;
                }
            }
            else {
                walk_chain(object_addr, object_size, baseaddr, segs, nsegs,
                           segs64, nsegs64, targets, ntarget, swapped, segInfo,
                           iseg, segname, pointer_format, pageIndex,
                           offsetInPage, dbi, ndbi);
            }
        }
    }

    free(targets);
}

/*
 * get_dyld_chained_fixups() unpacks the dyld bind info from the data pointed
 * to by an LC_DYLD_CHAINED_FIXUPS load command. Information is returned in
 * the older dyld_bind_info so that existing logic can be reused.
 */
void
get_dyld_chained_fixups(/* input */
                        const uint8_t* start,
                        const uint8_t* end,
                        const char **dylibs,
                        uint32_t ndylibs,
                        struct segment_command **segs,
                        uint32_t nsegs,
                        struct segment_command_64 **segs64,
                        uint32_t nsegs64,
                        enum bool swapped,
                        char *object_addr,
                        uint64_t object_size,
                        /* output */
                        struct dyld_bind_info **dbi,
                        uint64_t *ndbi,
                        enum chain_format_t *chain_format,
                        enum chain_header_t header_type,
                        enum bool print_errors)
{
    uint64_t baseaddr = get_base_addr(segs, nsegs, segs64, nsegs64);

    *chain_format = CHAIN_FORMAT_NONE;
    *dbi = NULL;
    *ndbi = 0;

    if (CHAIN_HEADER_SECTION == header_type) {
        get_dyld_chained_fixups_section(start, end, baseaddr, dylibs, ndylibs,
                                        segs, nsegs, segs64, nsegs64, swapped,
                                        object_addr, object_size, dbi, ndbi,
                                        chain_format);
    }
    else {
        get_dyld_chained_fixups_lcmd(start, end, baseaddr, dylibs, ndylibs,
                                     segs, nsegs, segs64, nsegs64, swapped,
                                     object_addr, object_size, dbi, ndbi,
                                     chain_format);
    }
}

uint64_t
get_chained_rebase_value(
uint64_t chain_value,
enum chain_format_t chain_format,
uint64_t textbase)
{
    uint64_t value = 0;
    enum bool is_rebase = FALSE;
    enum bool is_offset = FALSE;

    switch (chain_format) {
        case CHAIN_FORMAT_NONE:
            break;
        case CHAIN_FORMAT_PTR_ARM64E_KERNEL:
        case CHAIN_FORMAT_PTR_ARM64E_USERLAND:
        case CHAIN_FORMAT_PTR_ARM64E_USERLAND24:
            is_offset = TRUE;
            /* FALLTHROUGH */
        case CHAIN_FORMAT_ARM64E:
        case CHAIN_FORMAT_PTR_ARM64E_FIRMWARE:
        {
            struct dyld_chained_ptr_arm64e_auth_rebase* auth_rebase =
                (struct dyld_chained_ptr_arm64e_auth_rebase*)&chain_value;

            if (!auth_rebase->bind) {
                is_rebase = TRUE;
                if (auth_rebase->auth) {
                    value = auth_rebase->target;
                    is_offset = TRUE;
                }
                else {
                    struct dyld_chained_ptr_arm64e_rebase* rebase =
                        (struct dyld_chained_ptr_arm64e_rebase*)&chain_value;
                    value = (((uint64_t)rebase->high8 << 56) | rebase->target);
                }
            }
        }
            break;
        case CHAIN_FORMAT_PTR_64_OFFSET:
            is_offset = TRUE;
            /* FALLTHROUGH */
        case CHAIN_FORMAT_PTR_64:
        {
            struct dyld_chained_ptr_64_rebase* rebase =
            (struct dyld_chained_ptr_64_rebase*)&chain_value;
            if (!rebase->bind) {
                is_rebase = TRUE;
                value = (((uint64_t)rebase->high8 << 56) | rebase->target);
            }
        }
            break;
        case CHAIN_FORMAT_PTR_64_KERNEL_CACHE:
        case CHAIN_FORMAT_PTR_X86_64_KERNEL_CACHE:
        {
            struct dyld_chained_ptr_64_kernel_cache_rebase* rebase =
            (struct dyld_chained_ptr_64_kernel_cache_rebase*)&chain_value;
            is_rebase = TRUE;
            value = rebase->target;
        }
            break;
        case CHAIN_FORMAT_PTR_32:
        {
            struct dyld_chained_ptr_32_rebase* rebase =
            (struct dyld_chained_ptr_32_rebase*)&chain_value;
            if (!rebase->bind) {
                is_rebase = TRUE;
                value = rebase->target;
            }
        }
            break;
        case CHAIN_FORMAT_PTR_32_CACHE:
        {
            struct dyld_chained_ptr_32_cache_rebase* rebase =
            (struct dyld_chained_ptr_32_cache_rebase*)&chain_value;
            is_rebase = TRUE;
            value = rebase->target;
        }
            break;
        case CHAIN_FORMAT_PTR_32_FIRMWARE:
        {
            struct dyld_chained_ptr_32_firmware_rebase* rebase =
            (struct dyld_chained_ptr_32_firmware_rebase*)&chain_value;
            is_rebase = TRUE;
            value = rebase->target;
        }
            break;
    }

    if (is_rebase) {
        if (is_offset)
            value += textbase;
        return value;
    }
    
    return chain_value;
}

static
int
find_chained_fixup_header_offset(
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    uint64_t object_size,
    uint32_t* out_header_offset,
    uint32_t* out_header_size,
    enum chain_header_t* out_header_type,
    uint32_t* out_nseg,
    const char*** out_segs,
    uint32_t* out_ndylib,
    const char*** out_dylibs)
{
    enum byte_sex host_byte_sex;
    enum bool swapped, found_header;
    struct load_command *lc, l;
    struct linkedit_data_command chained_fixups;
    struct segment_command sg;
    struct segment_command_64 sg64;
    struct section s;
    struct section_64 s64;
    struct dylib_command dl;
    uint32_t i, j, left, size;
    enum chain_header_t header_type;
    uint32_t header_offset;
    uint32_t header_size;
    char* p;
    uint32_t nseg;
    const char** seg_names;
    uint32_t ndylib;
    const char** dylibs;
    char *short_name, *has_suffix;
    enum bool is_framework;

    host_byte_sex = get_host_byte_sex();
    swapped = host_byte_sex != load_commands_byte_sex;
    found_header = FALSE;
    header_type = CHAIN_HEADER_UNKNOWN;
    header_offset = header_size = 0;
    nseg = 0;
    seg_names = NULL;
    ndylib = 0;
    dylibs = NULL;

    /* find the chained fixup header location */
    lc = load_commands;
    for(i = 0 ; i < ncmds; i++){
        memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
        if(swapped)
            swap_load_command(&l, host_byte_sex);
        if(l.cmdsize % sizeof(int32_t) != 0)
            printf("load command %u size not a multiple of "
                   "sizeof(int32_t)\n", i);
        if((char *)lc + l.cmdsize >
           (char *)load_commands + sizeofcmds)
            printf("load command %u extends past end of load "
                   "commands\n", i);
        left = sizeofcmds - (uint32_t)((char *)lc - (char *)load_commands);

        switch(l.cmd){
            case LC_DYLD_CHAINED_FIXUPS:
                if(found_header == TRUE){
                    printf("more than one LC_DYLD_CHAINED_FIXUPS load commands "
                           "or __TEXT,__chain_starts sections\n");
                    return -1;
                }

                found_header = TRUE;
                if (!header_type)
                    header_type = CHAIN_HEADER_LOAD_COMMAND;

                memset((char *)&chained_fixups, '\0',
                       sizeof(struct linkedit_data_command));
                size = (left < sizeof(struct linkedit_data_command) ?
                        left : sizeof(struct linkedit_data_command));
                memcpy((char *)&chained_fixups, (char *)lc, size);
                if(swapped)
                    swap_linkedit_data_command(&chained_fixups, host_byte_sex);

                header_offset = chained_fixups.dataoff;
                header_size = chained_fixups.datasize;
                break;
            case LC_SEGMENT:
                memset((char *)&sg, '\0', sizeof(struct segment_command));
                size = left < sizeof(struct segment_command) ?
                left : sizeof(struct segment_command);
                memcpy((char *)&sg, (char *)lc, size);
                if(swapped)
                    swap_segment_command(&sg, host_byte_sex);

                seg_names = reallocf(seg_names, sizeof(char*) * nseg + 1);
                seg_names[nseg++] = strdup(sg.segname);

                if (0 == strcmp(sg.segname, "__TEXT")) {
                    p = (char *)lc + sizeof(struct segment_command);
                    for(j = 0 ; j < sg.nsects ; j++){
                        if(p + sizeof(struct section) >
                           (char *)load_commands + sizeofcmds){
                            printf("section structure command extends past "
                                   "end of load commands\n");
                            return -1;
                        }
                        left = sizeofcmds - (uint32_t)(p-(char *)load_commands);
                        memset((char *)&s, '\0', sizeof(struct section));
                        size = left < sizeof(struct section) ?
                        left : sizeof(struct section);
                        memcpy((char *)&s, p, size);
                        if(swapped)
                            swap_section(&s, 1, host_byte_sex);
                        p += size;

                        if (0 == strcmp(s.segname,  "__TEXT") &&
                            0 == strcmp(s.sectname, "__chain_starts")) {
                            if(found_header == TRUE){
                                printf("more than one LC_DYLD_CHAINED_FIXUPS "
                                       "load commands or __TEXT,__chain_starts "
                                       "sections\n");
                                return -1;
                            }

                            found_header = TRUE;
                            if (!header_type)
                                header_type = CHAIN_HEADER_SECTION;

                            header_offset = s.offset;
                            header_size = s.size;
                        }
                    }
                }
                break;
            case LC_SEGMENT_64:
                memset((char *)&sg64, '\0', sizeof(struct segment_command_64));
                size = left < sizeof(struct segment_command_64) ?
                left : sizeof(struct segment_command_64);
                memcpy((char *)&sg64, (char *)lc, size);
                if(swapped)
                    swap_segment_command_64(&sg64, host_byte_sex);

                seg_names = reallocf(seg_names, sizeof(char*) * nseg + 1);
                seg_names[nseg++] = strdup(sg64.segname);

                if (0 == strcmp(sg64.segname, "__TEXT")) {
                    p = (char *)lc + sizeof(struct segment_command_64);
                    for(j = 0 ; j < sg64.nsects ; j++){
                        if(p + sizeof(struct section_64) >
                           (char *)load_commands + sizeofcmds){
                            printf("section structure command extends past "
                                   "end of load commands\n");
                            return -1;
                        }
                        left = sizeofcmds - (uint32_t)(p-(char *)load_commands);
                        memset((char *)&s64, '\0', sizeof(struct section_64));
                        size = left < sizeof(struct section_64) ?
                        left : sizeof(struct section_64);
                        memcpy((char *)&s64, p, size);
                        if(swapped)
                            swap_section_64(&s64, 1, host_byte_sex);
                        p += size;

                        if (0 == strcmp(s64.segname,  "__TEXT") &&
                            0 == strcmp(s64.sectname, "__chain_starts")) {
                            if(found_header == TRUE){
                                printf("more than one LC_DYLD_CHAINED_FIXUPS "
                                       "load commands or __TEXT,__chain_starts "
                                       "sections\n");
                                return -1;
                            }

                            found_header = TRUE;
                            if (!header_type)
                                header_type = CHAIN_HEADER_SECTION;

                            header_offset = s64.offset;
                            header_size = (uint32_t)s64.size;
                        }
                    }
                }
                break;
            case LC_LOAD_DYLIB:
            case LC_LOAD_WEAK_DYLIB:
            case LC_REEXPORT_DYLIB:
            case LC_LOAD_UPWARD_DYLIB:
            case LC_LAZY_LOAD_DYLIB:
                if (out_ndylib || out_dylibs) {
                    memset((char *)&dl, '\0', sizeof(struct dylib_command));
                    size = (left < sizeof(struct dylib_command) ?
                            left : sizeof(struct dylib_command));
                    memcpy((char *)&dl, (char *)lc, size);
                    if(swapped)
                        swap_dylib_command(&dl, host_byte_sex);
                    short_name = NULL;
                    if(dl.dylib.name.offset < dl.cmdsize &&
                       dl.dylib.name.offset < left){
                        p = (char *)lc + dl.dylib.name.offset;
                        short_name = guess_short_name(p, &is_framework,
                                                      &has_suffix);
                    }
                    else
                        short_name = "(bad offset for dylib.name.offset)";

                    dylibs = reallocf(dylibs, sizeof(char*) * ndylib + 1);
                    dylibs[ndylib++] = strdup(short_name);
                }
                break;
            default:
                ;
        }

        if(l.cmdsize == 0){
            printf("load command %u size zero (can't advance to other "
                   "load commands)\n", i);
            break;
        }
        lc = (struct load_command *)((char *)lc + l.cmdsize);
        if((char *)lc > (char *)load_commands + sizeofcmds)
            break;
    }

    if (!found_header)
        return -1;

    if (out_header_offset)
        *out_header_offset = header_offset;
    if (out_header_size)
        *out_header_size = header_size;
    if (out_header_type)
        *out_header_type = header_type;
    if (out_nseg)
        *out_nseg = nseg;
    if (out_segs)
        *out_segs = seg_names;
    if (out_ndylib)
        *out_ndylib = ndylib;
    if (out_dylibs)
        *out_dylibs = dylibs;
    return 0;
}

static
const char* chain_pointer_format_name(uint32_t pointer_format)
{
    const char* table[] = {
        NULL,
        "DYLD_CHAINED_PTR_ARM64E",
        "DYLD_CHAINED_PTR_64",
        "DYLD_CHAINED_PTR_32",
        "DYLD_CHAINED_PTR_32_CACHE",
        "DYLD_CHAINED_PTR_32_FIRMWARE",
        "DYLD_CHAINED_PTR_64_OFFSET",
        "DYLD_CHAINED_PTR_ARM64E_KERNEL",
        "DYLD_CHAINED_PTR_64_KERNEL_CACHE",
        "DYLD_CHAINED_PTR_ARM64E_USERLAND",
        "DYLD_CHAINED_PTR_ARM64E_FIRMWARE",
        "DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE",
        "DYLD_CHAINED_PTR_ARM64E_USERLAND24",
    };
    const uint32_t count = sizeof(table) / sizeof(*table);
    if (pointer_format < count)
        return table[pointer_format];
    return NULL;
}

static
void
print_dyld_chained_fixups_lcmd(
char *object_addr,
uint64_t object_size,
uint32_t header_offset,
uint32_t header_size,
uint32_t nseg,
const char** segs,
uint32_t ndylib,
const char** dylibs,
enum bool swapped)
{
    if (sizeof(struct dyld_chained_fixups_header) > header_size) {
        printf("chained fixups header extends past end of data.\n");
        return;
    }
    const char* p = object_addr + header_offset;
    const char* header_addr = p;
    struct dyld_chained_fixups_header header;
    uint32_t left, size;

    left = (uint32_t)(object_size - (p - object_addr));
    memset((char *)&header, '\0', sizeof(header));
    size = left < sizeof(header) ? left : sizeof(header);
    memcpy((char *)&header, p, size);
    if(swapped) {
        header.fixups_version = _OSSwapInt32(header.fixups_version);
        header.starts_offset = _OSSwapInt32(header.starts_offset);
        header.imports_offset = _OSSwapInt32(header.imports_offset);
        header.symbols_offset = _OSSwapInt32(header.symbols_offset);
        header.imports_count = _OSSwapInt32(header.imports_count);
        header.imports_format = _OSSwapInt32(header.imports_format);
        header.symbols_format = _OSSwapInt32(header.symbols_format);
    }

    /* print the chained fixups header values */
    printf("chained fixups header (LC_DYLD_CHAINED_FIXUPS)\n");
    printf("  fixups_version = %d\n", header.fixups_version);
    printf("  starts_offset  = %d\n", header.starts_offset);
    printf("  imports_offset = %d\n", header.imports_offset);
    printf("  symbols_offset = %d\n", header.symbols_offset);
    printf("  imports_count  = %d\n", header.imports_count);

    printf("  imports_format = %d", header.imports_format);
    if (header.imports_format == DYLD_CHAINED_IMPORT)
        printf(" (DYLD_CHAINED_IMPORT)");
    else if (header.imports_format == DYLD_CHAINED_IMPORT_ADDEND)
        printf(" (DYLD_CHAINED_IMPORT_ADDEND)");
    else if (header.imports_format == DYLD_CHAINED_IMPORT_ADDEND64)
        printf(" (DYLD_CHAINED_IMPORT_ADDEND64)");
    printf("\n");

    printf("  symbols_format = %d", header.symbols_format);
    if (header.symbols_format == 1)
        printf(" (zlib compressed)");
    printf("\n");

    /* load the chained fixups image starts */
    if (header.starts_offset + sizeof(uint32_t) >= header_size) {
        printf("chained starts count begins past end of data.\n");
        return;
    }
    if (header.starts_offset >= header_size) {
        printf("chained starts count begins past end of data.\n");
        return;
    }
    if (header_offset + header.starts_offset >= object_size) {
        printf("chained starts count begins past end of file.\n");
        return;
    }
    if (header_offset + header.starts_offset + sizeof(uint32_t) >
        object_size) {
        printf("chained starts count extends past end of file.\n");
        return;
    }

    uint32_t seg_count;

    p = object_addr + header_offset + header.starts_offset;
    left = (uint32_t)(object_size - (p - object_addr));
    memset((char *)&seg_count, '\0', sizeof(seg_count));
    size = left < sizeof(seg_count) ? left : sizeof(seg_count);
    memcpy((char *)&seg_count, p, size);
    if(swapped) {
        seg_count = _OSSwapInt32(seg_count);
    }

    /* print the chained fixups image starts */
    printf("chained starts in image\n");
    printf("  seg_count = %d\n", seg_count);

    /* load and print the segment start offsets */
    p += sizeof(seg_count);
    for(int i = 0; i < seg_count; ++i) {
        const char* q = p + i * sizeof(uint32_t);
        if (q - header_addr >= header_size) {
            printf("    seg_offset[%d] begins past end of data.\n", i);
            continue;
        }
        if (q + sizeof(uint32_t) - header_addr > header_size) {
            printf("    seg_offset[%d] extends past end of data.\n",i);
            continue;
        }
        if (q - object_addr >= object_size) {
            printf("    seg_offset[%d] begins past end of file.\n", i);
            continue;
        }
        if (q + sizeof(uint32_t) - object_addr > object_size) {
            printf("    seg_offset[%d] extends past end of file.\n", i);
            continue;
        }
        uint32_t seg_offset = *(uint32_t*)q;
        if(swapped) {
            seg_offset = _OSSwapInt32(seg_offset);
        }
        printf("    seg_offset[%d] = %d (%s)\n", i, seg_offset, segs[i]);
    }

    /* load and print the segment starts and their contents */
    for(int i = 0; i < seg_count; ++i) {
        /* get the seg_offset again, without error reporting */
        p = object_addr + header_offset + header.starts_offset +
            sizeof(seg_count);
        if (p - object_addr >= object_size) {
            continue;
        }
        if (p + sizeof(uint32_t) - object_addr > object_size) {
            continue;
        }
        uint32_t seg_offset = *(uint32_t*)(p + i * sizeof(uint32_t));

        /* skip segments with an offset of 0 */
        if (0 == seg_offset) continue;

        p = object_addr + header_offset + header.starts_offset + seg_offset;
        struct dyld_chained_starts_in_segment starts;

        /* load the segment starts at index i */
        if (p - header_addr >= header_size) {
            printf("chained starts begin past end of data.\n");
            continue;
        }
        if (p + sizeof(uint32_t) - header_addr > header_size) {
            printf("chained starts extend past end of data.\n");
            continue;
        }
        if (p - object_addr >= object_size) {
            printf("chained starts begin past end of file.\n");
            return;
        }
        if (p + sizeof(starts) - object_addr >= object_size) {
            printf("chained starts extend past end of file.\n");
            return;
        }

        left = (uint32_t)(object_size - (p - object_addr));
        memset((char *)&starts, '\0', sizeof(starts));
        size = left < sizeof(starts) ? left : sizeof(starts);
        memcpy((char *)&starts, p, size);
        if(swapped) {
            starts.size = _OSSwapInt32(starts.size);
            starts.page_size = _OSSwapInt16(starts.page_size);
            starts.pointer_format = _OSSwapInt16(starts.pointer_format);
            starts.segment_offset = _OSSwapInt64(starts.segment_offset);
            starts.max_valid_pointer = _OSSwapInt32(starts.max_valid_pointer);
            starts.page_count = _OSSwapInt16(starts.page_count);
        }
        const char* name = chain_pointer_format_name(starts.pointer_format);

        /* print the segment starts at index i */

        printf("chained starts in segment %d", i);
        if (i < nseg)
            printf(" (%s)", segs[i]);
        printf("\n");

        printf("  size = %d\n", starts.size);
        printf("  page_size = 0x%x\n", starts.page_size);

        printf("  pointer_format = %d", starts.pointer_format);
        if (name) {
            printf(" (%s)", name);
        }
        printf("\n");

        printf("  segment_offset = 0x%llx\n", starts.segment_offset);
        printf("  max_valid_pointer = %d\n", starts.max_valid_pointer);
        printf("  page_count = %d\n", starts.page_count);

        /*
         * print the page starts in this segment. This data is spread across
         * two conceptual lists: a list of page starts, and a list of additional
         * chain starts for cases where a page has more than one starts. This
         * function will print the physical structure on disk, rather than
         * the logical structure. To do that, we'll assume all the space
         * reserved by start.size contains a valid uint16_t entry, and we'll
         * call it a page_start or a chain_start based on its index.
         */
        const char* q = p + sizeof(starts) - sizeof(uint16_t);
        for(int j = 0; q - p < starts.size; ++j, q += sizeof(uint16_t)) {
            const char* field = j < starts.page_count ?
                "page_start" : "chain_starts";
            if (q - header_addr >= header_size) {
                printf("    %s[%d] begins past end of data.\n",
                       field, j);
                continue;
            }
            if (q + sizeof(uint16_t) - header_addr > header_size) {
                printf("    %s[%d] extends past end of data.\n",
                       field, j);
                continue;
            }
            if (q - object_addr >= object_size) {
                printf("    %s[%d] begins past end of file.\n", field, j);
                continue;
            }
            if (q + sizeof(uint16_t) - object_addr >= object_size) {
                printf("    %s[%d] extends past end of file.\n", field, j);
                continue;
            }
            uint16_t start = 0;
            memcpy(&start, q, sizeof(start));
            if(swapped) {
                start = _OSSwapInt16(start);
            }
            printf("    %s[%d] = %d", field, j, start);
            if (start == DYLD_CHAINED_PTR_START_NONE)
                printf(" (DYLD_CHAINED_PTR_START_NONE)");
            else if (j < starts.page_count &&
                     start & DYLD_CHAINED_PTR_START_MULTI) {
                printf(" (%d DYLD_CHAINED_PTR_START_MULTI)",
                       start & ~DYLD_CHAINED_PTR_START_MULTI);
            }
            else if (start & DYLD_CHAINED_PTR_START_LAST) {
                printf(" (%d DYLD_CHAINED_PTR_START_LAST)",
                       start & ~DYLD_CHAINED_PTR_START_LAST);
            }
            printf("\n");
        }
    }

    /* load and print imports */
    char* strtab = object_addr + header_offset + header.symbols_offset;
    for (int i = 0; i < header.imports_count; ++i) {
        char* imports_start = (object_addr + header_offset +
                               header.imports_offset);
        int lib_ordinal;
        const char* dylibName;
        const char* symName;

        if (header.imports_format == DYLD_CHAINED_IMPORT) {
            uint32_t values[1];
            struct dyld_chained_import import;

            printf("dyld chained import[%d]", i);

            if (i * sizeof(import) >= header_size) {
                printf(" begins past end of data.\n");
                continue;
            }
            if (i * sizeof(import) + sizeof(import) > header_size) {
                printf(" extends past end of data.\n");
                continue;
            }

            memcpy(values, imports_start + i * sizeof(values), sizeof(values));
            if (swapped) {
                values[0] = _OSSwapInt32(values[0]);
            }
            printf(" = 0x%08x\n", values[0]);

            memcpy(&import, values, sizeof(import));

            /* convert the library ordinal into an int */
            lib_ordinal = libOrdinalFromUInt8(import.lib_ordinal);
            if (!validateOrdinal(lib_ordinal, dylibs, ndylib, &dylibName,
                                 NULL)) {
                dylibName = "bad dylib ordinal";
            }
            if (import.name_offset + header.symbols_offset < header_size)
                symName = &strtab[import.name_offset];
            else
                symName = "begins past end of data";

            printf("  lib_ordinal = %d (%s)\n", lib_ordinal, dylibName);
            printf("  weak_import = %d\n", import.weak_import);
            printf("  name_offset = %d (%s)\n", import.name_offset, symName);
        }
        else if (header.imports_format == DYLD_CHAINED_IMPORT_ADDEND) {
            uint32_t values[2];
            struct dyld_chained_import_addend import;

            printf("dyld chained import addend[%d]", i);

            if (i * sizeof(import) >= header_size) {
                printf(" begins past end of data.\n");
                continue;
            }
            if (i * sizeof(import) + sizeof(import) > header_size) {
                printf(" extends past end of data.\n");
                continue;
            }

            memcpy(values, imports_start + i * sizeof(values), sizeof(values));
            if (swapped) {
                values[0] = _OSSwapInt32(values[0]);
                values[1] = _OSSwapInt32(values[1]);
            }
            printf(" = 0x%08x 0x%08x\n", values[0], values[1]);

            memcpy(&import, values, sizeof(import));

            /* convert the library ordinal into an int */
            lib_ordinal = libOrdinalFromUInt8(import.lib_ordinal);
            if (!validateOrdinal(lib_ordinal, dylibs, ndylib, &dylibName,
                                 NULL)) {
                dylibName = "bad dylib ordinal";
            }
            if (import.name_offset + header.symbols_offset < header_size)
                symName = &strtab[import.name_offset];
            else
                symName = "begins past end of data";

            printf("  lib_ordinal = %d (%s)\n", lib_ordinal, dylibName);
            printf("  weak_import = %d\n", import.weak_import);
            printf("  name_offset = %d (%s)\n", import.name_offset, symName);
            printf("  addend      = %d\n", import.addend);
        }
        else if (header.imports_format == DYLD_CHAINED_IMPORT_ADDEND64) {
            uint64_t values[2];
            struct dyld_chained_import_addend64 import;

            printf("dyld chained import addend64[%d]", i);

            if (i * sizeof(import) >= header_size) {
                printf(" begins past end of data.\n");
                continue;
            }
            if (i * sizeof(import) + sizeof(import) > header_size) {
                printf(" extends past end of data.\n");
                continue;
            }

            memcpy(values, imports_start + i * sizeof(values), sizeof(values));
            if (swapped) {
                values[0] = _OSSwapInt64(values[0]);
                values[1] = _OSSwapInt64(values[1]);
            }
            printf(" = 0x%016llx 0x%016llx\n", values[0], values[1]);

            memcpy(&import, values, sizeof(import));

            /* convert the library ordinal into an int */
            lib_ordinal = libOrdinalFromUInt16(import.lib_ordinal);
            if (!validateOrdinal(lib_ordinal, dylibs, ndylib, &dylibName,
                                 NULL)) {
                dylibName = "bad dylib ordinal";
            }
            if (import.name_offset + header.symbols_offset < header_size)
                symName = &strtab[import.name_offset];
            else
                symName = "begins past end of data";

            printf("  lib_ordinal = %d (%s)\n", lib_ordinal, dylibName);
            printf("  weak_import = %d\n", import.weak_import);
            printf("  name_offset = %d (%s)\n", import.name_offset, symName);
            printf("  addend      = %lld\n", import.addend);
        }
    }
}

static
void
print_dyld_chained_fixups_section(
char *object_addr,
uint64_t object_size,
uint32_t header_offset,
uint32_t header_size,
enum bool swapped)
{
    /* get the chain starts header */
    if (sizeof(struct dyld_chained_starts_offsets) > header_size) {
        printf("chained fixups header extends past end of data.\n");
        return;
    }
    const char* p = object_addr + header_offset;
    const char* header_addr = p;
    struct dyld_chained_starts_offsets header;
    uint32_t left, size;

    left = (uint32_t)(object_size - (p - object_addr));
    memset((char *)&header, '\0', sizeof(header));
    size = left < sizeof(header) ? left : sizeof(header);
    memcpy((char *)&header, p, size);
    if(swapped) {
        header.pointer_format = _OSSwapInt32(header.pointer_format);
        header.starts_count = _OSSwapInt32(header.starts_count);
    }

    /* print the chained fixups header values */
    printf("chained fixups header (__TEXT,__chain_starts)\n");
    printf("  pointer_format = %d\n", header.pointer_format);
    printf("  starts_count   = %d\n", header.starts_count);

    /* print the chain starts */
    const char* q = p + sizeof(header) - sizeof(header.chain_starts);
    for(int i = 0; i < header.starts_count; ++i, q += sizeof(uint32_t)) {
        const char* field = "chain_starts";
        if (q - header_addr >= header_size) {
            printf("    %s[%d] begins past end of data.\n", field, i);
            continue;
        }
        if (q + sizeof(uint32_t) - header_addr > header_size) {
            printf("    %s[%d] extends past end of data.\n",
                   field, i);
            continue;
        }
        if (q - object_addr >= object_size) {
            printf("    %s[%d] begins past end of file.\n", field, i);
            continue;
        }
        if (q + sizeof(uint32_t) - object_addr >= object_size) {
            printf("    %s[%d] extends past end of file.\n", field, i);
            continue;
        }
        uint32_t start = 0;
        memcpy(&start, q, sizeof(start));
        if(swapped) {
            start = _OSSwapInt32(start);
        }
        printf("    %s[%d] = %d", field, i, start);
        printf("\n");
    }
}

/*
 * print_dyld_chained_fixups() looks for a LC_DYLD_CHAINED_FIXUPS load command
 * or chained fixup section and dumps its contents.
 */
void
print_dyld_chained_fixups(
struct load_command *load_commands,
uint32_t ncmds,
uint32_t sizeofcmds,
enum byte_sex byte_order,
char *object_addr,
uint64_t object_size)
{
    uint32_t header_offset;
    uint32_t header_size;
    enum chain_header_t header_type;
    uint32_t nseg;
    const char** segs;
    uint32_t ndylib;
    const char** dylibs;
    enum bool swapped;

    /*
     * find the chained fixup header. if there is no header offset or size
     * this is probably a dylib stub.
     */
    if (find_chained_fixup_header_offset(load_commands, ncmds, sizeofcmds,
                                         byte_order, object_addr, object_size,
                                         &header_offset, &header_size,
                                         &header_type, &nseg, &segs,
                                         &ndylib, &dylibs)) {
        return;
    }
    if (header_offset == 0 && header_size == 0)
        return;

    /* load the chained fixups header */
    if (header_offset >= object_size) {
        printf("chained fixups header starts past end of file.\n");
        return;
    }
    if (header_offset + header_size > object_size) {
        printf("chained fixups header extends past end of file.\n");
        return;
    }

    swapped = (get_host_byte_sex() != byte_order);
    if (CHAIN_HEADER_LOAD_COMMAND == header_type) {
        print_dyld_chained_fixups_lcmd(object_addr, object_size,
                                       header_offset, header_size,
                                       nseg, segs, ndylib, dylibs, swapped);
    }
    else if (CHAIN_HEADER_SECTION == header_type) {
        print_dyld_chained_fixups_section(object_addr, object_size,
                                          header_offset, header_size, swapped);
    }

    if (segs) {
        for (int i = 0; i < nseg; ++i)
            free((void*)segs[i]);
        free(segs);
    }
    if (dylibs) {
        for (int i = 0; i < ndylib; ++i)
            free((void*)dylibs[i]);
        free(dylibs);
    }
}
