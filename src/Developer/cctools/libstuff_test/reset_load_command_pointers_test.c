//
//  reset_load_command_pointers_test.c
//  libstuff_test
//
//  Created by Michael Trent on 5/11/20.
//

#include "test_main.h"

#include "stuff/breakout.h"

#include <mach-o/loader.h>
#include <stdlib.h>
#include <string.h>

/*
 * utilities for working with load commands
 *
 * TODO: Consolidate with align_test.c into a common utility.
 */

static void mh_add_buffer(struct mach_header* mh,
			  struct load_command** lcmds,
			  void* q,
			  uint32_t size)
{
    *lcmds = reallocf(*lcmds, mh->sizeofcmds + size);
    unsigned char* p = (unsigned char*)*lcmds;
    memcpy(&(p[mh->sizeofcmds]), q, size);
    mh->sizeofcmds += size;
}

static void mh_add_load_command(struct mach_header* mh,
				struct load_command** lcmds,
				void* lc,
				uint32_t size)
{
    mh_add_buffer(mh, lcmds, lc, size);
    mh->ncmds += 1;
}
/*
static void mh_add_segment32(struct mach_header* mh,
			     struct load_command** lcmds,
			     uint32_t vmaddr,
			     uint32_t nsects)
{
    struct segment_command sg = { 0 };
    sg.cmd = LC_SEGMENT;
    sg.vmaddr = vmaddr;
    sg.nsects = nsects;
    sg.cmdsize = (sizeof(struct segment_command) +
		  sizeof(struct section) * sg.nsects);

    mh_add_load_command(mh, lcmds, &sg, sizeof(sg));
}

static void mh_add_section32(struct mach_header* mh,
			   struct load_command** lcmds,
			   uint32_t align)
{
    struct section sc = {0};
    sc.align = align;
    mh_add_buffer(mh, lcmds, &sc, sizeof(sc));
}

static void mh_add_segment64(struct mach_header_64* mh,
			     struct load_command** lcmds,
			     uint32_t vmaddr,
			     uint32_t nsects)
{
    struct segment_command_64 sg = { 0 };
    sg.cmd = LC_SEGMENT_64;
    sg.vmaddr = vmaddr;
    sg.nsects = nsects;
    sg.cmdsize = (sizeof(struct segment_command_64) +
		  sizeof(struct section_64) * sg.nsects);

    mh_add_load_command((struct mach_header*)mh, lcmds, &sg, sizeof(sg));
}

static void mh_add_section64(struct mach_header_64* mh,
			     struct load_command** lcmds,
			     uint32_t align)
{
    struct section_64 sc = {0};
    sc.align = align;
    mh_add_buffer((struct mach_header*)mh, lcmds, &sc, sizeof(sc));
}
*/
static void mh_reset(void* mh_void, struct load_command** lcmds)
{
    struct mach_header* mh = (struct mach_header*)mh_void;
    if (*lcmds)
	free(*lcmds);
    *lcmds = NULL;
    mh->ncmds = 0;
    mh->sizeofcmds = 0;
}

/*
 * random32 and random64() are little helpers for generating random values
 * for testing
 */

static uint32_t random32(void)
{
    return arc4random();
}
static uint64_t random64(void)
{
    uint64_t r = arc4random();
    return (r << 32) | arc4random();
}

/* random_ledata_cmd() generates a random struct linkedit_data_command */

static struct linkedit_data_command
random_ledata_cmd(uint32_t cmd)
{
    struct linkedit_data_command lc = {0};
    lc.cmd = cmd;
    lc.cmdsize = sizeof(lc);
    lc.dataoff = random32();
    lc.datasize = random32();
    return lc;
}

/* enum describing each struct object load command index pointer */
enum index_type {
    index_type_none,
    index_type_seg_linkedit,
    index_type_seg_bitcode,
    index_type_symtab,
    index_type_dysymtab,
    index_type_dyldinfo,
    index_type_tlhint,
    index_type_prebind,
    index_type_codesig,
    index_type_splitseg,
    index_type_funcstarts,
    index_type_dice,
    index_type_csdir,
    index_type_ldhint,
    index_type_trie,
    index_type_chains,
    index_type_encinfo,
};

static int check_loadcmd(const enum index_type type, const char* name,
			 const void* orig, void* copy, const uint32_t size,
			 const enum index_type except)
{
    if (except == type)
	return check_null(name, copy);
    return check_memory(name, orig, copy, size);
}

#define UNINITIALIZED ((void*)0xA0A0A0A0)

static void test_reset_load_command_pointers(int is64, enum index_type except)
{
    struct object _obj, *obj = &_obj;
    struct mach_header mh32 = {0};
    struct mach_header_64 mh64 = {0};
    void* mh_void;

    /* initialze a stubbed out object. */
    memset(obj, 0, sizeof(*obj));
    if (is64)
	obj->mh64 = mh_void = &mh64;
    else
	obj->mh   = mh_void = &mh32;
    obj->st = UNINITIALIZED;
    obj->dyst = UNINITIALIZED;
    obj->hints_cmd = UNINITIALIZED;
    obj->cs = UNINITIALIZED;
    obj->seg_bitcode = UNINITIALIZED;
    obj->seg_bitcode64 = UNINITIALIZED;
    obj->seg_linkedit = UNINITIALIZED;
    obj->seg_linkedit64 = UNINITIALIZED;
    obj->code_sig_cmd = UNINITIALIZED;
    obj->split_info_cmd = UNINITIALIZED;
    obj->func_starts_info_cmd = UNINITIALIZED;
    obj->data_in_code_cmd = UNINITIALIZED;
    obj->code_sign_drs_cmd = UNINITIALIZED;
    obj->link_opt_hint_cmd = UNINITIALIZED;
    obj->dyld_info = UNINITIALIZED;
    obj->dyld_exports_trie = UNINITIALIZED;
    obj->dyld_chained_fixups = UNINITIALIZED;
    obj->encryption_info_command = UNINITIALIZED;
    obj->encryption_info_command64 = UNINITIALIZED;

    /* add a LINKEDIT segment */
    struct segment_command seg_linkedit32 = {0};
    struct segment_command seg_bitcode32 = {0};
    struct segment_command_64 seg_linkedit64 = {0};
    struct segment_command_64 seg_bitcode64 = {0};
    if (is64) {
	if (except != index_type_seg_linkedit) {
	    strcpy(seg_linkedit64.segname, SEG_LINKEDIT);
	    seg_linkedit64.vmaddr = random64();
	    seg_linkedit64.vmsize = random64();
	    seg_linkedit64.fileoff = random64();
	    seg_linkedit64.filesize = random64();
	    seg_linkedit64.cmd = LC_SEGMENT_64;
	    seg_linkedit64.cmdsize = (sizeof(struct segment_command_64) +
				    sizeof(struct section_64) *
				    seg_linkedit64.nsects);
	    mh_add_load_command(mh_void, &obj->load_commands, &seg_linkedit64,
				sizeof(seg_linkedit64));
	}

	if (except != index_type_seg_bitcode) {
	    strcpy(seg_bitcode64.segname, "__LLVM");
	    seg_bitcode64.vmaddr = random64();
	    seg_bitcode64.vmsize = random64();
	    seg_bitcode64.fileoff = random64();
	    seg_bitcode64.filesize = random64();
	    seg_bitcode64.cmd = LC_SEGMENT_64;
	    seg_bitcode64.cmdsize = (sizeof(struct segment_command_64) +
				     sizeof(struct section_64) *
				     seg_bitcode64.nsects);
	    mh_add_load_command(mh_void, &obj->load_commands, &seg_bitcode64,
				sizeof(seg_bitcode64));
	}
    }
    else {
	if (except != index_type_seg_linkedit) {
	    strcpy(seg_linkedit32.segname, SEG_LINKEDIT);
	    seg_linkedit32.vmaddr = random32();
	    seg_linkedit32.vmsize = random32();
	    seg_linkedit32.fileoff = random32();
	    seg_linkedit32.filesize = random32();
	    seg_linkedit32.cmd = LC_SEGMENT;
	    seg_linkedit32.cmdsize = (sizeof(struct segment_command) +
				      sizeof(struct section) *
				      seg_linkedit32.nsects);
	    mh_add_load_command(mh_void, &obj->load_commands, &seg_linkedit32,
				sizeof(seg_linkedit32));
	}

	if (except != index_type_seg_bitcode) {
	    strcpy(seg_bitcode32.segname, "__LLVM");
	    seg_bitcode32.vmaddr = random32();
	    seg_bitcode32.vmsize = random32();
	    seg_bitcode32.fileoff = random32();
	    seg_bitcode32.filesize = random32();
	    seg_bitcode32.cmd = LC_SEGMENT;
	    seg_bitcode32.cmdsize = (sizeof(struct segment_command) +
				     sizeof(struct section) *
				     seg_bitcode32.nsects);
	    mh_add_load_command(mh_void, &obj->load_commands, &seg_bitcode32,
				sizeof(seg_bitcode32));
	}
    }

    /* add a symtab */
    struct symtab_command st = {0};
    if (except != index_type_symtab) {
	st.cmd = LC_SYMTAB;
	st.cmdsize = sizeof(st);
	st.symoff = random32();
	st.nsyms = random32();
	st.stroff = random32();
	st.strsize = random32();
	mh_add_load_command(mh_void, &obj->load_commands, &st, sizeof(st));
    }

    /* add a dysymtab */
    struct dysymtab_command dyst = {0};
    if (except != index_type_dysymtab) {
	dyst.cmd = LC_DYSYMTAB;
	dyst.cmdsize = sizeof(dyst);
	dyst.ilocalsym = random32();
	dyst.nlocalsym = random32();
	dyst.iextdefsym = random32();
	dyst.nextdefsym = random32();
	dyst.iundefsym = random32();
	dyst.nundefsym = random32();
	dyst.tocoff = random32();
	dyst.ntoc = random32();
	dyst.modtaboff = random32();
	dyst.nmodtab = random32();
	dyst.extrefsymoff = random32();
	dyst.nextrefsyms = random32();
	dyst.indirectsymoff = random32();
	dyst.nindirectsyms = random32();
	dyst.extreloff = random32();
	dyst.nextrel = random32();
	dyst.locreloff = random32();
	dyst.nlocrel = random32();
	mh_add_load_command(mh_void, &obj->load_commands, &dyst, sizeof(dyst));
    }

    /* add dyld info */
    struct dyld_info_command di = {0};
    if (except != index_type_dyldinfo) {
	di.cmd = LC_DYLD_INFO_ONLY;
	di.cmdsize = sizeof(di);
	di.rebase_off = random32();
	di.rebase_size = random32();
	di.bind_off = random32();
	di.bind_size = random32();
	di.weak_bind_off = random32();
	di.weak_bind_size = random32();
	di.lazy_bind_off = random32();
	di.lazy_bind_size = random32();
	di.export_off = random32();
	di.export_size = random32();
	mh_add_load_command(mh_void, &obj->load_commands, &di, sizeof(di));
    }

    /* add a hints command */
    struct twolevel_hints_command tlhint = {0};
    if (except != index_type_tlhint) {
	tlhint.cmd = LC_TWOLEVEL_HINTS;
	tlhint.cmdsize = sizeof(tlhint);
	tlhint.offset = random32();
	tlhint.nhints = random32();
	mh_add_load_command(mh_void, &obj->load_commands, &tlhint,
			    sizeof(tlhint));
    }

    /* add a prebind checksum command */
    struct prebind_cksum_command pbcs = {0};
    if (except != index_type_prebind) {
	pbcs.cmd = LC_PREBIND_CKSUM;
	pbcs.cmdsize = sizeof(pbcs);
	pbcs.cksum = random32();
	mh_add_load_command(mh_void, &obj->load_commands, &pbcs, sizeof(pbcs));
    }

    /* add a code signature command */
    struct linkedit_data_command cs = random_ledata_cmd(LC_CODE_SIGNATURE);
    if (except != index_type_codesig)
	mh_add_load_command(mh_void, &obj->load_commands, &cs, sizeof(cs));

    /* add a split-seg info command */
    struct linkedit_data_command ss = random_ledata_cmd(LC_SEGMENT_SPLIT_INFO);
    if (except != index_type_splitseg)
	mh_add_load_command(mh_void, &obj->load_commands, &ss, sizeof(ss));

    /* add a function starts command */
    struct linkedit_data_command fs = random_ledata_cmd(LC_FUNCTION_STARTS);
    if (except != index_type_funcstarts)
	mh_add_load_command(mh_void, &obj->load_commands, &fs, sizeof(fs));

    /* add a data in code command */
    struct linkedit_data_command dice = random_ledata_cmd(LC_DATA_IN_CODE);
    if (except != index_type_dice)
	mh_add_load_command(mh_void, &obj->load_commands, &dice, sizeof(dice));

    /* add code sign directives */
    struct linkedit_data_command csdr =
	random_ledata_cmd(LC_DYLIB_CODE_SIGN_DRS);
    if (except != index_type_csdir)
	mh_add_load_command(mh_void, &obj->load_commands, &csdr, sizeof(csdr));

    /* add linker hints */
    struct linkedit_data_command ldhint =
	random_ledata_cmd(LC_LINKER_OPTIMIZATION_HINT);
    if (except != index_type_ldhint)
	mh_add_load_command(mh_void, &obj->load_commands, &ldhint,
			    sizeof(ldhint));

    /* add dyld export trie command */
    struct linkedit_data_command et = random_ledata_cmd(LC_DYLD_EXPORTS_TRIE);
    if (except != index_type_trie)
	mh_add_load_command(mh_void, &obj->load_commands, &et, sizeof(et));

    /* add dyld chained fixups command */
    struct linkedit_data_command cf = random_ledata_cmd(LC_DYLD_CHAINED_FIXUPS);
    if (except != index_type_chains)
	mh_add_load_command(mh_void, &obj->load_commands, &cf, sizeof(cf));

    /* add encryption info */
    struct encryption_info_command enc32 = {0};
    struct encryption_info_command_64 enc64 = {0};
    if (except != index_type_encinfo) {
	if (is64) {
	    enc64.cmd = LC_ENCRYPTION_INFO_64;
	    enc64.cmdsize = sizeof(enc64);
	    enc64.cryptoff = random32();
	    enc64.cryptsize = random32();
	    enc64.cryptid = random32();
	    enc64.pad = random32();
	    mh_add_load_command(mh_void, &obj->load_commands, &enc64,
				sizeof(enc64));
	}
	else {
	    enc32.cmd = LC_ENCRYPTION_INFO;
	    enc32.cmdsize = sizeof(enc32);
	    enc32.cryptoff = random32();
	    enc32.cryptsize = random32();
	    enc32.cryptid = random32();
	    mh_add_load_command(mh_void, &obj->load_commands, &enc32,
				sizeof(enc32));
	}
    }

    /*
     * OK!
     *
     * Now call reset_load_command_pointers() and verify the indexes are set!
     */
    reset_load_command_pointers(obj);

    if (is64) {
	check_null("obj->seg_linkedit", obj->seg_linkedit);
	check_null("obj->seg_bitcode", obj->seg_bitcode);
	check_loadcmd(index_type_seg_linkedit, "linkedit", &seg_linkedit64,
		      obj->seg_linkedit64, sizeof(seg_linkedit64), except);
	check_loadcmd(index_type_seg_bitcode, "bitcode", &seg_bitcode64,
		      obj->seg_bitcode64, sizeof(seg_bitcode64), except);
    }
    else {
	check_null("obj->seg_linkedit64", obj->seg_linkedit64);
	check_null("obj->seg_bitcode64", obj->seg_bitcode64);
	check_loadcmd(index_type_seg_linkedit, "linkedit", &seg_linkedit32,
		      obj->seg_linkedit, sizeof(seg_linkedit32), except);
	check_loadcmd(index_type_seg_bitcode, "bitcode", &seg_bitcode32,
		      obj->seg_bitcode, sizeof(seg_bitcode32), except);
    }

    check_loadcmd(index_type_symtab, "symtab", &st, obj->st,
		  sizeof(st), except);
    check_loadcmd(index_type_dysymtab, "dysymtab", &dyst, obj->dyst,
		  sizeof(dyst), except);
    check_loadcmd(index_type_dyldinfo, "dyld info", &di, obj->dyld_info,
		  sizeof(di), except);
    check_loadcmd(index_type_tlhint, "two level hints", &tlhint, obj->hints_cmd,
		  sizeof(tlhint), except);
    check_loadcmd(index_type_prebind, "prebind cksum", &pbcs, obj->cs,
		  sizeof(pbcs), except);
    check_loadcmd(index_type_codesig, "code signature", &cs, obj->code_sig_cmd,
		  sizeof(cs), except);
    check_loadcmd(index_type_splitseg, "split seg", &ss, obj->split_info_cmd,
		  sizeof(ss), except);
    check_loadcmd(index_type_funcstarts, "function starts", &fs,
		  obj->func_starts_info_cmd,  sizeof(fs), except);
    check_loadcmd(index_type_dice, "data in code", &dice,
		  obj->data_in_code_cmd,  sizeof(dice), except);
    check_loadcmd(index_type_csdir, "codesign directives", &csdr,
		  obj->code_sign_drs_cmd,  sizeof(csdr), except);
    check_loadcmd(index_type_ldhint, "linker hints", &ldhint,
		  obj->link_opt_hint_cmd,  sizeof(ldhint), except);
    check_loadcmd(index_type_trie, "dyld exports trie", &et,
		  obj->dyld_exports_trie,  sizeof(et), except);
    check_loadcmd(index_type_chains, "dyld chained fixups", &cf,
		  obj->dyld_chained_fixups,  sizeof(cf), except);

    if (is64) {
	check_null("obj->encryption_info_command",
		   obj->encryption_info_command);
	check_loadcmd(index_type_encinfo, "encryption info", &enc64,
		      obj->encryption_info_command64,  sizeof(enc64), except);
    }
    else {
	check_null("obj->encryption_info_command64",
		   obj->encryption_info_command64);
	check_loadcmd(index_type_encinfo, "encryption info", &enc32,
		      obj->encryption_info_command,  sizeof(enc32), except);
    }

    /* clean up */
    mh_reset(mh_void, &obj->load_commands);
}

static void test_reset_all(uint32_t is64)
{
    enum index_type exceptions[] = {
	index_type_none,
	index_type_seg_linkedit,
	index_type_seg_bitcode,
	index_type_symtab,
	index_type_dysymtab,
	index_type_dyldinfo,
	index_type_tlhint,
	index_type_prebind,
	index_type_codesig,
	index_type_splitseg,
	index_type_funcstarts,
	index_type_dice,
	index_type_csdir,
	index_type_ldhint,
	index_type_trie,
	index_type_chains,
	index_type_encinfo,
    };
    uint32_t n = sizeof(exceptions) / sizeof(*exceptions);
    for (uint32_t i = 0; i < n; ++i) {
	test_reset_load_command_pointers(is64, exceptions[i]);
    }
}

static void test_reset_32(void)
{
    test_reset_all(0);
}

static void test_reset_64(void)
{
    test_reset_all(1);
}

static int test_main(void)
{
    int err = 0;

    if (!err) err = test_add("test reset_load_command_pointers 32-bit",
			     test_reset_32);
    if (!err) err = test_add("test reset_load_command_pointers 64-bit",
			     test_reset_64);
    if (!err) err = test_add("test removing code signature load command",
			     test_reset_64);

    return err;
}
