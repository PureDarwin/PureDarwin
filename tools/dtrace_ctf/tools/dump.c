/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#if !defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/mman.h>
#else
#include <sys/types.h>
//#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "darwin_shim.h"
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/stab.h>

int debug_level;
const char *progname = "ctfdump";
#endif /* __APPLE__ */

#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <gelf.h>
#include <zlib.h>

#include "ctf_headers.h"
#include "strtab.h"
#include "ctftools.h"
#include "utils.h"
#include "symbol.h"

extern caddr_t write_buffer(ctf_header_t *h, ctf_buf_t *buf, size_t *resszp);
extern caddr_t write_compressed_buffer(ctf_header_t *h, ctf_buf_t *buf, size_t *resszp);
struct ctf_buf {
	strtab_t ctb_strtab;	/* string table */
	caddr_t ctb_base;	/* pointer to base of buffer */
	caddr_t ctb_end;	/* pointer to end of buffer */
	caddr_t ctb_ptr;	/* pointer to empty buffer space */
	size_t ctb_size;	/* size of buffer */
	int nptent;		/* number of processed types */
	int ntholes;		/* number of type holes */
};

void write_file(Elf *src, const char *srcname, Elf *dst, const char *dstname, caddr_t ctfdata, size_t ctfsize, int flags);

#define	WARN(x)	{ warn(x); return (E_ERROR); }

/*
 * Flags that indicate what data is to be displayed.  An explicit `all' value is
 * provided to allow the code to distinguish between a request for everything
 * (currently requested by invoking ctfdump without flags) and individual
 * requests for all of the types of data (an invocation with all flags).  In the
 * former case, we want to be able to implicitly adjust the definition of `all'
 * based on the CTF version of the file being dumped.  For example, if a v2 file
 * is being dumped, `all' includes F_LABEL - a request to dump the label
 * section.  If a v1 file is being dumped, `all' does not include F_LABEL,
 * because v1 CTF doesn't support labels.  We need to be able to distinguish
 * between `ctfdump foo', which has an implicit request for labels if `foo'
 * supports them, and `ctfdump -l foo', which has an explicity request.  In the
 * latter case, we exit with an error if `foo' is a v1 CTF file.
 */
static enum {
	F_DATA	= 0x01,		/* show data object section */
	F_FUNC	= 0x02,		/* show function section */
	F_HDR	= 0x04,		/* show header */
	F_STR	= 0x08,		/* show string table */
	F_TYPES	= 0x10,		/* show type section */
	F_STATS = 0x20, 	/* show statistics */
	F_LABEL	= 0x40,		/* show label section */
	F_ALL	= 0x80,		/* explicit request for `all' */
	F_ALLMSK = 0xff		/* show all sections and statistics */
} flags = 0;

static struct {
	ulong_t s_ndata;	/* total number of data objects */
	ulong_t s_nfunc;	/* total number of functions */
	ulong_t s_nargs;	/* total number of function arguments */
	ulong_t s_argmax;	/* longest argument list */
	ulong_t s_ntypes;	/* total number of types */
	ulong_t s_types[16];	/* number of types by kind */
	ulong_t s_nsmem;	/* total number of struct members */
	ulong_t s_nsbytes;	/* total size of all structs */
	ulong_t s_smmax;	/* largest struct in terms of members */
	ulong_t s_sbmax;	/* largest struct in terms of bytes */
	ulong_t s_numem;	/* total number of union members */
	ulong_t s_nubytes;	/* total size of all unions */
	ulong_t s_ummax;	/* largest union in terms of members */
	ulong_t s_ubmax;	/* largest union in terms of bytes */
	ulong_t s_nemem;	/* total number of enum members */
	ulong_t s_emmax;	/* largest enum in terms of members */
	ulong_t s_nstr;		/* total number of strings */
	size_t s_strlen;	/* total length of all strings */
	size_t s_strmax;	/* longest string length */
} stats;

typedef struct ctf_data {
	caddr_t cd_ctfdata;	/* Pointer to the CTF data */
	size_t cd_ctflen;	/* Length of CTF data */

	/*
	 * cd_symdata will be non-NULL if the CTF data is being retrieved from
	 * an ELF file with a symbol table.  cd_strdata and cd_nsyms should be
	 * used only if cd_symdata is non-NULL.
	 */
	Elf_Data *cd_symdata;	/* Symbol table */
	Elf_Data *cd_strdata;	/* Symbol table strings */
	int cd_nsyms;		/* Number of symbol table entries */
#if defined (__APPLE__)
	Elf32_Word sh_link;
#endif /* __APPLE__ */
} ctf_data_t;

static const char *
ref_to_str(uint_t name, const ctf_header_t *hp, const ctf_data_t *cd)
{
	size_t offset = CTF_NAME_OFFSET(name);
	const char *s = cd->cd_ctfdata + hp->cth_stroff + offset;

	if (CTF_NAME_STID(name) != CTF_STRTAB_0)
		return ("<< ??? - name in external strtab >>");

	if (offset >= hp->cth_strlen)
		return ("<< ??? - name exceeds strlab len >>");

	if (hp->cth_stroff + offset >= cd->cd_ctflen)
		return ("<< ??? - file truncated >>");

	if (s[0] == '\0')
		return ("(anon)");

	return (s);
}

static const char *
int_encoding_to_str(uint_t encoding)
{
	static char buf[32];

	if (encoding == 0 || (encoding & ~(CTF_INT_SIGNED | CTF_INT_CHAR |
	    CTF_INT_BOOL | CTF_INT_VARARGS)) != 0)
		(void) snprintf(buf, sizeof (buf), " 0x%x", encoding);
	else {
		buf[0] = '\0';
		if (encoding & CTF_INT_SIGNED)
			(void) strcat(buf, " SIGNED");
		if (encoding & CTF_INT_CHAR)
			(void) strcat(buf, " CHAR");
		if (encoding & CTF_INT_BOOL)
			(void) strcat(buf, " BOOL");
		if (encoding & CTF_INT_VARARGS)
			(void) strcat(buf, " VARARGS");
	}

	return (buf + 1);
}

static const char *
fp_encoding_to_str(uint_t encoding)
{
	static const char *const encs[] = {
		NULL, "SINGLE", "DOUBLE", "COMPLEX", "DCOMPLEX", "LDCOMPLEX",
		"LDOUBLE", "INTERVAL", "DINTERVAL", "LDINTERVAL", "IMAGINARY",
		"DIMAGINARY", "LDIMAGINARY"
	};

	static char buf[16];

	if (encoding < 1 || encoding >= (sizeof (encs) / sizeof (char *))) {
		(void) snprintf(buf, sizeof (buf), "%u", encoding);
		return (buf);
	}

	return (encs[encoding]);
}

static void
print_line(const char *s)
{
	static const char line[] = "----------------------------------------"
	    "----------------------------------------";
	(void) printf("\n%s%.*s\n\n", s, (int)(78 - strlen(s)), line);
}

static int
print_header(const ctf_header_t *hp, const ctf_data_t *cd)
{
	print_line("- CTF Header ");

	(void) printf("  cth_magic    = 0x%04x\n", hp->cth_magic);
	(void) printf("  cth_version  = %u\n", hp->cth_version);
	(void) printf("  cth_flags    = 0x%02x\n", hp->cth_flags);
	(void) printf("  cth_parlabel = %s\n",
	    ref_to_str(hp->cth_parlabel, hp, cd));
	(void) printf("  cth_parname  = %s\n",
	    ref_to_str(hp->cth_parname, hp, cd));
	(void) printf("  cth_lbloff   = %u\n", hp->cth_lbloff);
	(void) printf("  cth_objtoff  = %u\n", hp->cth_objtoff);
	(void) printf("  cth_funcoff  = %u\n", hp->cth_funcoff);
	(void) printf("  cth_typeoff  = %u\n", hp->cth_typeoff);
	(void) printf("  cth_stroff   = %u\n", hp->cth_stroff);
	(void) printf("  cth_strlen   = %u\n", hp->cth_strlen);

	return (E_SUCCESS);
}

static int
print_labeltable(const ctf_header_t *hp, const ctf_data_t *cd)
{
	/* LINTED - pointer alignment */
	const ctf_lblent_t *ctl = (ctf_lblent_t *)(cd->cd_ctfdata +
	    hp->cth_lbloff);
	ulong_t i, n = (hp->cth_objtoff - hp->cth_lbloff) / sizeof (*ctl);

	print_line("- Label Table ");

	if (hp->cth_lbloff & 3)
		WARN("cth_lbloff is not aligned properly\n");
	if (hp->cth_lbloff >= cd->cd_ctflen)
		WARN("file is truncated or cth_lbloff is corrupt\n");
	if (hp->cth_objtoff >= cd->cd_ctflen)
		WARN("file is truncated or cth_objtoff is corrupt\n");
	if (hp->cth_lbloff > hp->cth_objtoff)
		WARN("file is corrupt -- cth_lbloff > cth_objtoff\n");

	for (i = 0; i < n; i++, ctl++) {
		(void) printf("  %5u %s\n", ctl->ctl_typeidx,
		    ref_to_str(ctl->ctl_label, hp, cd));
	}

	return (E_SUCCESS);
}

#if defined(__APPLE__)
static GElf_Sym *
gelf_getsym_macho(Elf_Data * data, int ndx, GElf_Sym * sym, const char *base)
{
	const struct nlist *nsym = (const struct nlist *)(data->d_buf);
	const char *name;
	char *tmp;

	nsym += ndx;
	name = base + nsym->n_un.n_strx;

	if (0 == nsym->n_un.n_strx) // iff a null, "", name.
		name = "null name"; // return NULL;

	if ('_' == name[0])
		name++; // Lop off omnipresent underscore to match DWARF convention

	sym->st_name = (GElf_Sxword)(name - base);
	sym->st_value = nsym->n_value;
	sym->st_size = 0;
	sym->st_info = GELF_ST_INFO((STB_GLOBAL), (STT_NOTYPE));
	sym->st_other = 0;
	sym->st_shndx = SHN_MACHO; /* Mark underlying file as Mach-o */

	if (nsym->n_type & N_STAB) { /* Detect C++ methods */

		switch(nsym->n_type) {
		case N_FUN:
			sym->st_info = GELF_ST_INFO((STB_GLOBAL), (STT_FUNC));
			break;
		case N_GSYM:
			sym->st_info = GELF_ST_INFO((STB_GLOBAL), (STT_OBJECT));
			break;
		default:
			break;
		}

	} else if ((N_ABS | N_EXT) == (nsym->n_type & (N_TYPE | N_EXT)) ||
		(N_SECT | N_EXT) == (nsym->n_type & (N_TYPE | N_EXT))) {

		sym->st_info = GELF_ST_INFO((STB_GLOBAL), (nsym->n_desc));
	} else if ((N_UNDF | N_EXT) == (nsym->n_type & (N_TYPE | N_EXT)) &&
				nsym->n_sect == NO_SECT) {
		sym->st_info = GELF_ST_INFO((STB_GLOBAL), (STT_OBJECT)); /* Common */
	}

	return sym;
}

static GElf_Sym *
gelf_getsym_macho_64(Elf_Data * data, int ndx, GElf_Sym * sym, const char *base)
{
	const struct nlist_64 *nsym = (const struct nlist_64 *)(data->d_buf);
	const char *name;
	char *tmp;

	nsym += ndx;
	name = base + nsym->n_un.n_strx;

	if (0 == nsym->n_un.n_strx) // iff a null, "", name.
		name = "null name"; // return NULL;

	if ('_' == name[0])
		name++; // Lop off omnipresent underscore to match DWARF convention

	sym->st_name = (GElf_Sxword)(name - base);
	sym->st_value = nsym->n_value;
	sym->st_size = 0;
	sym->st_info = GELF_ST_INFO((STB_GLOBAL), (STT_NOTYPE));
	sym->st_other = 0;
	sym->st_shndx = SHN_MACHO_64; /* Mark underlying file as Mach-o */

	if (nsym->n_type & N_STAB) { /* Detect C++ methods */

		switch(nsym->n_type) {
		case N_FUN:
			sym->st_info = GELF_ST_INFO((STB_GLOBAL), (STT_FUNC));
			break;
		case N_GSYM:
			sym->st_info = GELF_ST_INFO((STB_GLOBAL), (STT_OBJECT));
			break;
		default:
			break;
		}

	} else if ((N_ABS | N_EXT) == (nsym->n_type & (N_TYPE | N_EXT)) ||
		(N_SECT | N_EXT) == (nsym->n_type & (N_TYPE | N_EXT))) {

		sym->st_info = GELF_ST_INFO((STB_GLOBAL), (nsym->n_desc));
	} else if ((N_UNDF | N_EXT) == (nsym->n_type & (N_TYPE | N_EXT)) &&
				nsym->n_sect == NO_SECT) {
		sym->st_info = GELF_ST_INFO((STB_GLOBAL), (STT_OBJECT)); /* Common */
	}

	return sym;
}
#endif /* __APPLE__ */

/*
 * Given the current symbol index (-1 to start at the beginning of the symbol
 * table) and the type of symbol to match, this function returns the index of
 * the next matching symbol (if any), and places the name of that symbol in
 * *namep.  If no symbol is found, -1 is returned.
 */
static int
next_sym(const ctf_data_t *cd, const int symidx, const uchar_t matchtype,
    char **namep)
{
	int i;

	for (i = symidx + 1; i < cd->cd_nsyms; i++) {
		GElf_Sym sym;
		char *name;
		int type;

#if defined(__APPLE__)
		if (cd->sh_link == SHN_MACHO) { /* Underlying file is Mach-o */
			if (gelf_getsym_macho(cd->cd_symdata, i, &sym, (const char *)(cd->cd_strdata->d_buf)) == 0)
				return (-1);
		}
		else if (cd->sh_link == SHN_MACHO_64) { /* Underlying file is Mach-o 64 */
			if (gelf_getsym_macho_64(cd->cd_symdata, i, &sym, (const char *)(cd->cd_strdata->d_buf)) == 0)
				return (-1);
		}
		else
#endif /* __APPLE__ */
		if (gelf_getsym(cd->cd_symdata, i, &sym) == 0)
			return (-1);

		name = (char *)cd->cd_strdata->d_buf + sym.st_name;
		type = GELF_ST_TYPE(sym.st_info);

		/*
		 * Skip various types of symbol table entries.
		 */
		if (type != matchtype || ignore_symbol(&sym, name))
			continue;

		/* Found one */
		*namep = name;
		return (i);
	}

	return (-1);
}

static int
read_data(const ctf_header_t *hp, const ctf_data_t *cd)
{
	/* LINTED - pointer alignment */
	const ushort_t *idp = (ushort_t *)(cd->cd_ctfdata + hp->cth_objtoff);
	ulong_t n = (hp->cth_funcoff - hp->cth_objtoff) / sizeof (ushort_t);

	if (flags != F_STATS)
		print_line("- Data Objects ");

	if (hp->cth_objtoff & 1)
		WARN("cth_objtoff is not aligned properly\n");
	if (hp->cth_objtoff >= cd->cd_ctflen)
		WARN("file is truncated or cth_objtoff is corrupt\n");
	if (hp->cth_funcoff >= cd->cd_ctflen)
		WARN("file is truncated or cth_funcoff is corrupt\n");
	if (hp->cth_objtoff > hp->cth_funcoff)
		WARN("file is corrupt -- cth_objtoff > cth_funcoff\n");

	if (flags != F_STATS) {
		int symidx, len, i;
		char *name = NULL;

		for (symidx = -1, i = 0; i < n; i++) {
			int nextsym;

			if (cd->cd_symdata == NULL || (nextsym = next_sym(cd,
				symidx, STT_OBJECT, &name)) < 0)
				name = NULL;
			else
				symidx = nextsym;

			len = printf("  [%u] %u", i, *idp++);
			if (name != NULL)
				(void) printf("%*s%s (%u)", (15 - len), "",
				    name, symidx);
			(void) putchar('\n');
		}
	}

	stats.s_ndata = n;
	return (E_SUCCESS);
}

static int
read_funcs(const ctf_header_t *hp, const ctf_data_t *cd)
{
	/* LINTED - pointer alignment */
	const ushort_t *fp = (ushort_t *)(cd->cd_ctfdata + hp->cth_funcoff);

	/* LINTED - pointer alignment */
	const ushort_t *end = (ushort_t *)(cd->cd_ctfdata + hp->cth_typeoff);

	ulong_t id;
	int symidx;

	if (flags != F_STATS)
		print_line("- Functions ");

	if (hp->cth_funcoff & 1)
		WARN("cth_funcoff is not aligned properly\n");
	if (hp->cth_funcoff >= cd->cd_ctflen)
		WARN("file is truncated or cth_funcoff is corrupt\n");
	if (hp->cth_typeoff >= cd->cd_ctflen)
		WARN("file is truncated or cth_typeoff is corrupt\n");
	if (hp->cth_funcoff > hp->cth_typeoff)
		WARN("file is corrupt -- cth_funcoff > cth_typeoff\n");

	for (symidx = -1, id = 0; fp < end; id++) {
		ushort_t info = *fp++;
		ushort_t kind = CTF_INFO_KIND(info);
		ushort_t n = CTF_INFO_VLEN(info);
		ushort_t i;
		int nextsym;
		char *name;

		if (cd->cd_symdata == NULL || (nextsym = next_sym(cd, symidx,
		    STT_FUNC, &name)) < 0)
			name = NULL;
		else
			symidx = nextsym;

		if (kind == CTF_K_UNKNOWN && n == 0)
			continue; /* skip padding */

		if (kind != CTF_K_FUNCTION) {
			(void) printf("  [%lu] unexpected kind -- %u\n",
			    id, kind);
			return (E_ERROR);
		}

		if (fp + n > end) {
			(void) printf("  [%lu] vlen %u extends past section "
			    "boundary\n", id, n);
			return (E_ERROR);
		}

		if (flags != F_STATS) {
			(void) printf("  [%lu] FUNC ", id);
			if (name != NULL)
				(void) printf("(%s) ", name);
			(void) printf("returns: %u args: (", *fp++);

			if (n != 0) {
				(void) printf("%u", *fp++);
				for (i = 1; i < n; i++)
					(void) printf(", %u", *fp++);
			}

			(void) printf(")\n");
		} else
			fp += n + 1; /* skip to next function definition */

		stats.s_nfunc++;
		stats.s_nargs += n;
		stats.s_argmax = MAX(stats.s_argmax, n);
	}

	return (E_SUCCESS);
}

static int
read_types(const ctf_header_t *hp, const ctf_data_t *cd)
{
	/* LINTED - pointer alignment */
	const ctf_type_t *tp = (ctf_type_t *)(cd->cd_ctfdata + hp->cth_typeoff);

	/* LINTED - pointer alignment */
	const ctf_type_t *end = (ctf_type_t *)(cd->cd_ctfdata + hp->cth_stroff);

	ulong_t id;

	if (flags != F_STATS)
		print_line("- Types ");

	if (hp->cth_typeoff & 3)
		WARN("cth_typeoff is not aligned properly\n");
	if (hp->cth_typeoff >= cd->cd_ctflen)
		WARN("file is truncated or cth_typeoff is corrupt\n");
	if (hp->cth_stroff >= cd->cd_ctflen)
		WARN("file is truncated or cth_stroff is corrupt\n");
	if (hp->cth_typeoff > hp->cth_stroff)
		WARN("file is corrupt -- cth_typeoff > cth_stroff\n");

	id = 1;
	if (hp->cth_parlabel || hp->cth_parname)
		id += 1 << CTF_PARENT_SHIFT;

	for (/* */; tp < end; id++) {
		ulong_t i, n = CTF_INFO_VLEN(tp->ctt_info);
		size_t size, increment, vlen = 0;
		int kind = CTF_INFO_KIND(tp->ctt_info);

		union {
			const void *ptr;
			const ctf_array_t *ap;
			const ctf_member_t *mp;
			const ctf_lmember_t *lmp;
			const ctf_enum_t *ep;
			const ushort_t *argp;
		} u;

		if (flags != F_STATS) {
			(void) printf("  %c%lu%c ",
			    "[<"[CTF_INFO_ISROOT(tp->ctt_info)], id,
			    "]>"[CTF_INFO_ISROOT(tp->ctt_info)]);
		}

		if (tp->ctt_size == CTF_LSIZE_SENT) {
			increment = sizeof (ctf_type_t);
			size = (size_t)CTF_TYPE_LSIZE(tp);
		} else {
			increment = sizeof (ctf_stype_t);
			size = tp->ctt_size;
		}
		u.ptr = (caddr_t)tp + increment;

		switch (kind) {
		case CTF_K_INTEGER:
			if (flags != F_STATS) {
				uint_t encoding = *((const uint_t *)u.ptr);

				(void) printf("INTEGER %s encoding=%s offset=%u"
				    " bits=%u", ref_to_str(tp->ctt_name, hp,
				    cd), int_encoding_to_str(
				    CTF_INT_ENCODING(encoding)),
				    CTF_INT_OFFSET(encoding),
				    CTF_INT_BITS(encoding));
			}
			vlen = sizeof (uint_t);
			break;

		case CTF_K_FLOAT:
			if (flags != F_STATS) {
				uint_t encoding = *((const uint_t *)u.ptr);

				(void) printf("FLOAT %s encoding=%s offset=%u "
				    "bits=%u", ref_to_str(tp->ctt_name, hp,
				    cd), fp_encoding_to_str(
				    CTF_FP_ENCODING(encoding)),
				    CTF_FP_OFFSET(encoding),
				    CTF_FP_BITS(encoding));
			}
			vlen = sizeof (uint_t);
			break;

		case CTF_K_POINTER:
			if (flags != F_STATS) {
				(void) printf("POINTER %s refers to %u",
				    ref_to_str(tp->ctt_name, hp, cd),
				    tp->ctt_type);
			}
			break;

		case CTF_K_ARRAY:
			if (flags != F_STATS) {
				(void) printf("ARRAY %s content: %u index: %u "
				    "nelems: %u\n", ref_to_str(tp->ctt_name,
				    hp, cd), u.ap->cta_contents,
				    u.ap->cta_index, u.ap->cta_nelems);
			}
			vlen = sizeof (ctf_array_t);
			break;

		case CTF_K_FUNCTION:
			if (flags != F_STATS) {
				(void) printf("FUNCTION %s returns: %u args: (",
				    ref_to_str(tp->ctt_name, hp, cd),
				    tp->ctt_type);

				if (n != 0) {
					(void) printf("%u", *u.argp++);
					for (i = 1; i < n; i++, u.argp++)
						(void) printf(", %u", *u.argp);
				}

				(void) printf(")");
			}

			vlen = sizeof (ushort_t) * (n + (n & 1));
			break;

		case CTF_K_STRUCT:
		case CTF_K_UNION:
			if (kind == CTF_K_STRUCT) {
				stats.s_nsmem += n;
				stats.s_smmax = MAX(stats.s_smmax, n);
				stats.s_nsbytes += size;
				stats.s_sbmax = MAX(stats.s_sbmax, size);

				if (flags != F_STATS)
					(void) printf("STRUCT");
			} else {
				stats.s_numem += n;
				stats.s_ummax = MAX(stats.s_ummax, n);
				stats.s_nubytes += size;
				stats.s_ubmax = MAX(stats.s_ubmax, size);

				if (flags != F_STATS)
					(void) printf("UNION");
			}

			if (flags != F_STATS) {
				(void) printf(" %s (%d bytes)\n",
				    ref_to_str(tp->ctt_name, hp, cd), size);

				if (size >= CTF_LSTRUCT_THRESH) {
					for (i = 0; i < n; i++, u.lmp++) {
						(void) printf(
						    "\t%s type=%u off=%llu\n",
						    ref_to_str(u.lmp->ctlm_name,
						    hp, cd), u.lmp->ctlm_type,
						    CTF_LMEM_OFFSET(u.lmp));
					}
				} else {
					for (i = 0; i < n; i++, u.mp++) {
						(void) printf(
						    "\t%s type=%u off=%u\n",
						    ref_to_str(u.mp->ctm_name,
						    hp, cd), u.mp->ctm_type,
						    u.mp->ctm_offset);
					}
				}
			}

			vlen = n * (size >= CTF_LSTRUCT_THRESH ?
			    sizeof (ctf_lmember_t) : sizeof (ctf_member_t));
			break;

		case CTF_K_ENUM:
			if (flags != F_STATS) {
				(void) printf("ENUM %s\n",
				    ref_to_str(tp->ctt_name, hp, cd));

				for (i = 0; i < n; i++, u.ep++) {
					(void) printf("\t%s = %d\n",
					    ref_to_str(u.ep->cte_name, hp, cd),
					    u.ep->cte_value);
				}
			}

			stats.s_nemem += n;
			stats.s_emmax = MAX(stats.s_emmax, n);

			vlen = sizeof (ctf_enum_t) * n;
			break;

		case CTF_K_FORWARD:
			if (flags != F_STATS) {
				(void) printf("FORWARD %s",
				    ref_to_str(tp->ctt_name, hp, cd));
			}
			break;

		case CTF_K_TYPEDEF:
			if (flags != F_STATS) {
				(void) printf("TYPEDEF %s refers to %u",
				    ref_to_str(tp->ctt_name, hp, cd),
				    tp->ctt_type);
			}
			break;

		case CTF_K_VOLATILE:
			if (flags != F_STATS) {
				(void) printf("VOLATILE %s refers to %u",
				    ref_to_str(tp->ctt_name, hp, cd),
				    tp->ctt_type);
			}
			break;

		case CTF_K_CONST:
			if (flags != F_STATS) {
				(void) printf("CONST %s refers to %u",
				    ref_to_str(tp->ctt_name, hp, cd),
				    tp->ctt_type);
			}
			break;

		case CTF_K_RESTRICT:
			if (flags != F_STATS) {
				(void) printf("RESTRICT %s refers to %u",
				    ref_to_str(tp->ctt_name, hp, cd),
				    tp->ctt_type);
			}
			break;

		case CTF_K_PTRAUTH:
			if (flags != F_STATS) {
				uint_t data = *((const uint_t *)u.ptr);

				(void) printf("PTRAUTH %s refers to %u key %d discriminated %llu discriminator %llu",
				    ref_to_str(tp->ctt_name, hp, cd),
				    tp->ctt_type, CTF_PTRAUTH_KEY(data),
				    CTF_PTRAUTH_DISCRIMINATED(data),
				    CTF_PTRAUTH_DISCRIMINATOR(data));
			}
			vlen = sizeof (uint_t);
			break;

		case CTF_K_UNKNOWN:
			break; /* hole in type id space */

		default:
			(void) printf("unexpected kind %u\n", kind);
			return (E_ERROR);
		}

		if (flags != F_STATS)
			(void) printf("\n");

		stats.s_ntypes++;
		stats.s_types[kind]++;

		tp = (ctf_type_t *)((uintptr_t)tp + increment + vlen);
	}

	return (E_SUCCESS);
}

static int
read_strtab(const ctf_header_t *hp, const ctf_data_t *cd)
{
	size_t n, off, len = hp->cth_strlen;
	const char *s = cd->cd_ctfdata + hp->cth_stroff;

	if (flags != F_STATS)
		print_line("- String Table ");

	if (hp->cth_stroff >= cd->cd_ctflen)
		WARN("file is truncated or cth_stroff is corrupt\n");
	if (hp->cth_stroff + hp->cth_strlen > cd->cd_ctflen)
		WARN("file is truncated or cth_strlen is corrupt\n");

	for (off = 0; len != 0; off += n) {
		if (flags != F_STATS) {
			(void) printf("  [%lu] %s\n", (ulong_t)off,
			    s[0] == '\0' ? "\\0" : s);
		}
		n = strlen(s) + 1;
		len -= n;
		s += n;

		stats.s_nstr++;
		stats.s_strlen += n;
		stats.s_strmax = MAX(stats.s_strmax, n);
	}

	return (E_SUCCESS);
}

/*
 * Remove unwanted symbols from strtab: replace them with "-    ".
 */
static int
clean_strtab(ctf_header_t *hp, ctf_data_t *cd, strtab_t *strtab,
		char **syms, int nsyms)
{
	size_t n, off, len = hp->cth_strlen;
	char *s = cd->cd_ctfdata + hp->cth_stroff;
	int i;

	char buf[1024];
	buf[0] = '-';
	memset(buf + 1, ' ', 1023);

	if (hp->cth_stroff >= cd->cd_ctflen)
		WARN("file is truncated or cth_stroff is corrupt\n");
	if (hp->cth_stroff + hp->cth_strlen > cd->cd_ctflen)
		WARN("file is truncated or cth_strlen is corrupt\n");

	for (off = 0; len != 0; off += n) {
		for (i = 0; i < nsyms; i++) {
			/* clean out offending string */
			if (!strcmp(s, syms[i]))
				strncpy(s, buf, strlen(s));
		}
		/* add all strings to fresh strtab */
		strtab_insert(strtab, atom_get(s));
		n = strlen(s) + 1;
		len -= n;
		s += n;
	}

	return 0;
}

/*
 * Convert from one representation to another of the ctf buffer
 */
static void
ctf_data_to_ctf_buf(ctf_header_t *hp, ctf_data_t *cd, ctf_buf_t *cb)
{
	/* create empty strtab (to be filled in clean_strtab) */
	strtab_create(&cb->ctb_strtab);
	cb->ctb_base = cd->cd_ctfdata;
	cb->ctb_end = cd->cd_ctfdata + hp->cth_stroff;
	cb->ctb_ptr = cb->ctb_end;
	cb->ctb_size = cd->cd_ctflen - hp->cth_strlen;
	/* not used for our purposes */
	cb->nptent = 0;
	cb->ntholes = 0;
}

/*
 * Similar to write_ctf, but we don't have to reconstruct the whole CTF section
 * from a tdata_t.
 */
static int
write_ctfdata(int srcfd, Elf *srcelf, const char *srcname, const char *dstname, ctf_header_t *hp, ctf_buf_t *buf)
{
	struct stat st;
	Elf *dstelf = NULL;
	caddr_t data = NULL;
	size_t len = 0;
	int dstfd;

	/* open + elf_begin destination file */
	fstat(srcfd, &st);
	if ((dstfd = open(dstname, O_RDWR | O_CREAT | O_TRUNC, st.st_mode)) < 0)
		terminate("Cannot open temp file %s for writing", dstname);
	if ((dstelf = elf_begin(dstfd, ELF_C_WRITE, NULL)) == NULL)
		elfterminate(dstname, "Cannot write");

	/* write out and/or compress header & data buffer */
	if (hp->cth_flags & CTF_F_COMPRESS) {
		data = write_compressed_buffer(hp, buf, &len);
	} else {
		data = write_buffer(hp, buf, &len);
		/*
		data = (caddr_t) xmalloc(sizeof(ctf_header_t) + cd->cd_ctflen);
		bcopy(hp, data, sizeof(ctf_header_t));
		len += sizeof(ctf_header_t);
		bcopy(cd->cd_ctfdata, data + len, cd->cd_ctflen);
		len += cd->cd_ctflen;
		*/
	}

	/* send the data to the destination file */
	write_file(srcelf, srcname, dstelf, dstname, data, len, 0);

	free(data);
	elf_end(dstelf);
	(void) close(dstfd);
	return 0;
}

static void
long_stat(const char *name, ulong_t value)
{
	(void) printf("  %-36s= %lu\n", name, value);
}

static void
fp_stat(const char *name, float value)
{
	(void) printf("  %-36s= %.2f\n", name, value);
}

static int
print_stats(void)
{
	print_line("- CTF Statistics ");

	long_stat("total number of data objects", stats.s_ndata);
	(void) printf("\n");

	long_stat("total number of functions", stats.s_nfunc);
	long_stat("total number of function arguments", stats.s_nargs);
	long_stat("maximum argument list length", stats.s_argmax);

	if (stats.s_nfunc != 0) {
		fp_stat("average argument list length",
		    (float)stats.s_nargs / (float)stats.s_nfunc);
	}

	(void) printf("\n");

	long_stat("total number of types", stats.s_ntypes);
	long_stat("total number of integers", stats.s_types[CTF_K_INTEGER]);
	long_stat("total number of floats", stats.s_types[CTF_K_FLOAT]);
	long_stat("total number of pointers", stats.s_types[CTF_K_POINTER]);
	long_stat("total number of arrays", stats.s_types[CTF_K_ARRAY]);
	long_stat("total number of func types", stats.s_types[CTF_K_FUNCTION]);
	long_stat("total number of structs", stats.s_types[CTF_K_STRUCT]);
	long_stat("total number of unions", stats.s_types[CTF_K_UNION]);
	long_stat("total number of enums", stats.s_types[CTF_K_ENUM]);
	long_stat("total number of forward tags", stats.s_types[CTF_K_FORWARD]);
	long_stat("total number of typedefs", stats.s_types[CTF_K_TYPEDEF]);
	long_stat("total number of volatile types",
	    stats.s_types[CTF_K_VOLATILE]);
	long_stat("total number of const types", stats.s_types[CTF_K_CONST]);
	long_stat("total number of restrict types",
	    stats.s_types[CTF_K_RESTRICT]);
	long_stat("total number of encoded pointers", stats.s_types[CTF_K_PTRAUTH]);
	long_stat("total number of unknowns (holes)",
	    stats.s_types[CTF_K_UNKNOWN]);

	(void) printf("\n");

	long_stat("total number of struct members", stats.s_nsmem);
	long_stat("maximum number of struct members", stats.s_smmax);
	long_stat("total size of all structs", stats.s_nsbytes);
	long_stat("maximum size of a struct", stats.s_sbmax);

	if (stats.s_types[CTF_K_STRUCT] != 0) {
		fp_stat("average number of struct members",
		    (float)stats.s_nsmem / (float)stats.s_types[CTF_K_STRUCT]);
		fp_stat("average size of a struct", (float)stats.s_nsbytes /
		    (float)stats.s_types[CTF_K_STRUCT]);
	}

	(void) printf("\n");

	long_stat("total number of union members", stats.s_numem);
	long_stat("maximum number of union members", stats.s_ummax);
	long_stat("total size of all unions", stats.s_nubytes);
	long_stat("maximum size of a union", stats.s_ubmax);

	if (stats.s_types[CTF_K_UNION] != 0) {
		fp_stat("average number of union members",
		    (float)stats.s_numem / (float)stats.s_types[CTF_K_UNION]);
		fp_stat("average size of a union", (float)stats.s_nubytes /
		    (float)stats.s_types[CTF_K_UNION]);
	}

	(void) printf("\n");

	long_stat("total number of enum members", stats.s_nemem);
	long_stat("maximum number of enum members", stats.s_emmax);

	if (stats.s_types[CTF_K_ENUM] != 0) {
		fp_stat("average number of enum members",
		    (float)stats.s_nemem / (float)stats.s_types[CTF_K_ENUM]);
	}

	(void) printf("\n");

	long_stat("total number of unique strings", stats.s_nstr);
	long_stat("bytes of string data", stats.s_strlen);
	long_stat("maximum string length", stats.s_strmax);

	if (stats.s_nstr != 0) {
		fp_stat("average string length",
		    (float)stats.s_strlen / (float)stats.s_nstr);
	}

	(void) printf("\n");
	return (E_SUCCESS);
}

static int
print_usage(FILE *fp, int verbose)
{
#if !defined(__APPLE__)
	(void) fprintf(fp, "Usage: %s [-dfhlsSt] [-u file] file\n", getpname());
#else
	(void) fprintf(fp, "Usage: %s [-dfhlrsSt] [-u file] file\n", progname);
#endif /* __APPLE__ */

	if (verbose) {
		(void) fprintf(fp,
		    "\t-d  dump data object section\n"
		    "\t-f  dump function section\n"
		    "\t-h  dump file header\n"
		    "\t-l  dump label table\n"
#ifdef __APPLE__
		    "\t-r  remove listed symbols from CTF data\n"
#endif /* __APPLE__ */
		    "\t-s  dump string table\n"
		    "\t-S  dump statistics\n"
		    "\t-t  dump type section\n"
		    "\t-u  save uncompressed CTF to a file\n");
	}

	return (E_USAGE);
}

static Elf_Scn *
findelfscn(Elf *elf, GElf_Ehdr *ehdr, char *secname)
{
	GElf_Shdr shdr;
	Elf_Scn *scn;
	char *name;

	for (scn = NULL; (scn = elf_nextscn(elf, scn)) != NULL; ) {
		if (gelf_getshdr(scn, &shdr) != NULL && (name =
		    elf_strptr(elf, ehdr->e_shstrndx, shdr.sh_name)) != NULL &&
		    strcmp(name, secname) == 0)
			return (scn);
	}

	return (NULL);
}

int
main(int argc, char *argv[])
{
	const char *filename = NULL;
	const char *ufile = NULL;
	int error = 0, doclean = 0;
	int c, fd, ufd;

	ctf_data_t cd;
	const ctf_preamble_t *pp;
	ctf_header_t *hp;
	Elf *elf;
	GElf_Ehdr ehdr;

	(void) elf_version(EV_CURRENT);

	for (opterr = 0; optind < argc; optind++) {
		while ((c = getopt(argc, argv, "dfhlrsStu:")) != (int)EOF) {
			switch (c) {
			case 'd':
				flags |= F_DATA;
				break;
			case 'f':
				flags |= F_FUNC;
				break;
			case 'h':
				flags |= F_HDR;
				break;
			case 'l':
				flags |= F_LABEL;
				break;
			case 'r':
				doclean = 1;
				goto skiploop;
			case 's':
				flags |= F_STR;
				break;
			case 'S':
				flags |= F_STATS;
				break;
			case 't':
				flags |= F_TYPES;
				break;
			case 'u':
				ufile = optarg;
				break;
			default:
				if (optopt == '?')
					return (print_usage(stdout, 1));
				warn("illegal option -- %c\n", optopt);
				return (print_usage(stderr, 0));
			}
		}

		if (optind < argc) {
			if (filename != NULL)
				return (print_usage(stderr, 0));
			filename = argv[optind];
		}
	}

skiploop:
	if (doclean)
		filename = argv[argc - 1];

	if (filename == NULL)
		return (print_usage(stderr, 0));

	if (flags == 0 && ufile == NULL)
		flags = F_ALLMSK;

	if ((fd = open(filename, O_RDONLY)) == -1)
		die("failed to open %s", filename);

	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) != NULL &&
	    gelf_getehdr(elf, &ehdr) != NULL) {

		Elf_Data *dp;
		Elf_Scn *ctfscn = findelfscn(elf, &ehdr, ".SUNW_ctf");
		Elf_Scn *symscn;
		GElf_Shdr ctfshdr;

		if (ctfscn == NULL || (dp = elf_getdata(ctfscn, NULL)) == NULL)
			die("%s does not contain .SUNW_ctf data\n", filename);

		cd.cd_ctfdata = dp->d_buf;
		cd.cd_ctflen = dp->d_size;

		/*
		 * If the sh_link field of the CTF section header is non-zero
		 * it indicates which section contains the symbol table that
		 * should be used. We default to the .symtab section if sh_link
		 * is zero or if there's an error reading the section header.
		 */
#if !defined(__APPLE__)
		if (gelf_getshdr(ctfscn, &ctfshdr) != NULL &&
		    ctfshdr.sh_link != 0) {
			symscn = elf_getscn(elf, ctfshdr.sh_link);
		} else {
			symscn = findelfscn(elf, &ehdr, ".symtab");
		}
#else
		if (gelf_getshdr(ctfscn, &ctfshdr) != NULL &&
		    ctfshdr.sh_link != 0 && ctfshdr.sh_link != SHN_MACHO && ctfshdr.sh_link != SHN_MACHO_64) {
			symscn = elf_getscn(elf, ctfshdr.sh_link);
		} else {
			symscn = findelfscn(elf, &ehdr, ".symtab");
		}
#endif /* __APPLE__ */

		/* If we found a symbol table, find the corresponding strings */
		if (symscn != NULL) {
			GElf_Shdr shdr;
			Elf_Scn *symstrscn;

			if (gelf_getshdr(symscn, &shdr) != NULL) {
#if !defined(__APPLE__)
				symstrscn = elf_getscn(elf, shdr.sh_link);

				cd.cd_nsyms = shdr.sh_size / shdr.sh_entsize;
				cd.cd_symdata = elf_getdata(symscn, NULL);
				cd.cd_strdata = elf_getdata(symstrscn, NULL);
#else
				if (SHN_MACHO == shdr.sh_link) { /* Underlying file is Mach-o */
					int dir_idx;
					cd.cd_nsyms = shdr.sh_size / shdr.sh_entsize;
					cd.cd_symdata = elf_getdata(symscn, NULL);

					if ((dir_idx = findelfsecidx(elf, filename, ".dir_str_table")) < 0 ||
						(symstrscn = elf_getscn(elf, dir_idx)) == NULL ||
						(cd.cd_strdata = elf_getdata(symstrscn, NULL)) == NULL)
						terminate("%s: Can't open direct string table\n", filename);

					cd.sh_link = SHN_MACHO; /* Mark underlying file as Mach-o */
				} else if (SHN_MACHO_64 == shdr.sh_link) { /* Underlying file is Mach-o 64 */
					int dir_idx;
					cd.cd_nsyms = shdr.sh_size / shdr.sh_entsize;
					cd.cd_symdata = elf_getdata(symscn, NULL);

					if ((dir_idx = findelfsecidx(elf, filename, ".dir_str_table")) < 0 ||
						(symstrscn = elf_getscn(elf, dir_idx)) == NULL ||
						(cd.cd_strdata = elf_getdata(symstrscn, NULL)) == NULL)
						terminate("%s: Can't open direct string table\n", filename);

					cd.sh_link = SHN_MACHO_64; /* Mark underlying file as Mach-o 64 */
				} else {
					symstrscn = elf_getscn(elf, shdr.sh_link);

					cd.cd_nsyms = shdr.sh_size / shdr.sh_entsize;
					cd.cd_symdata = elf_getdata(symscn, NULL);
					cd.cd_strdata = elf_getdata(symstrscn, NULL);
					cd.sh_link = shdr.sh_link;
				}
#endif /* __APPLE__ */
			}
		}
	} else {
		struct stat st;

		if (fstat(fd, &st) == -1)
			die("failed to fstat %s", filename);

		cd.cd_ctflen = st.st_size;
		cd.cd_ctfdata = mmap(NULL, cd.cd_ctflen, PROT_READ,
		    MAP_PRIVATE, fd, 0);
		if (cd.cd_ctfdata == MAP_FAILED)
			die("failed to mmap %s", filename);
	}

	/*
	 * Get a pointer to the CTF data buffer and interpret the first portion
	 * as a ctf_header_t.  Validate the magic number and size.
	 */

	if (cd.cd_ctflen < sizeof (ctf_preamble_t))
		die("%s does not contain a CTF preamble\n", filename);

	/* LINTED - pointer alignment */
	pp = (const ctf_preamble_t *)cd.cd_ctfdata;

	if (pp->ctp_magic != CTF_MAGIC)
		die("%s does not appear to contain CTF data\n", filename);

	if (pp->ctp_version == CTF_VERSION_2 ||
	    pp->ctp_version == CTF_VERSION_3) {
		/* LINTED - pointer alignment */
		hp = (ctf_header_t *)cd.cd_ctfdata;
		cd.cd_ctfdata = (caddr_t)cd.cd_ctfdata + sizeof (ctf_header_t);

		if (cd.cd_ctflen < sizeof (ctf_header_t)) {
			die("%s does not contain a v%d CTF header\n", filename,
			    CTF_VERSION);
		}
		/* Offset the start, so decrease the size by the same amount */
		cd.cd_ctflen -= sizeof(ctf_header_t);

	} else {
		die("%s contains unsupported CTF version %d\n", filename,
		    pp->ctp_version);
	}

	/*
	 * If the data buffer is compressed, then malloc a buffer large enough
	 * to hold the decompressed data, and use zlib to decompress it.
	 */
	if (hp->cth_flags & CTF_F_COMPRESS) {
		z_stream zstr;
		void *buf;
		int rc;

		if ((buf = malloc(hp->cth_stroff + hp->cth_strlen)) == NULL)
			die("failed to allocate decompression buffer");

		bzero(&zstr, sizeof (z_stream));
		zstr.next_in = (void *)cd.cd_ctfdata;
		zstr.avail_in = cd.cd_ctflen;
		zstr.next_out = buf;
		zstr.avail_out = hp->cth_stroff + hp->cth_strlen;

		if ((rc = inflateInit(&zstr)) != Z_OK)
			die("failed to initialize zlib: %s\n", zError(rc));

		if ((rc = inflate(&zstr, Z_FINISH)) != Z_STREAM_END)
			die("failed to decompress CTF data: %s\n", zError(rc));

		if ((rc = inflateEnd(&zstr)) != Z_OK)
			die("failed to finish decompression: %s\n", zError(rc));

		if (zstr.total_out != hp->cth_stroff + hp->cth_strlen)
			die("CTF data is corrupt -- short decompression\n");

		cd.cd_ctfdata = buf;
		cd.cd_ctflen = hp->cth_stroff + hp->cth_strlen;
	}

#ifdef __APPLE__
	if (doclean) {
		int nsyms = argc - 1 - optind;
		char **syms = &argv[optind];
		char *tmpname = (char *)mktmpname(filename, ".ctf");
		ctf_buf_t tmpbuf;

		ctf_data_to_ctf_buf(hp, &cd, &tmpbuf);
		clean_strtab(hp, &cd, &tmpbuf.ctb_strtab, syms, nsyms);

		write_ctfdata(fd, elf, filename, tmpname, hp, &tmpbuf);
		if (rename(tmpname, filename) != 0) {
			terminate("Couldn't rename temp file %s to %s", tmpname,
					filename);
		}
		free(tmpname);
		flags = 0;
	}
#endif /* __APPLE__ */

	if (flags & F_HDR)
		error |= print_header(hp, &cd);
	if (flags & (F_LABEL))
		error |= print_labeltable(hp, &cd);
	if (flags & (F_DATA | F_STATS))
		error |= read_data(hp, &cd);
	if (flags & (F_FUNC | F_STATS))
		error |= read_funcs(hp, &cd);
	if (flags & (F_TYPES | F_STATS))
		error |= read_types(hp, &cd);
	if (flags & (F_STR | F_STATS))
		error |= read_strtab(hp, &cd);
	if (flags & F_STATS)
		error |= print_stats();

	/*
	 * If the -u option is specified, write the uncompressed CTF data to a
	 * raw CTF file.  CTF data can already be extracted compressed by
	 * applying elfdump -w -N .SUNW_ctf to an ELF file, so we don't bother.
	 */
	if (ufile != NULL) {
		ctf_header_t h;

		bcopy(hp, &h, sizeof (h));
		h.cth_flags &= ~CTF_F_COMPRESS;

		if ((ufd = open(ufile, O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ||
		    write(ufd, &h, sizeof (h)) != sizeof (h) ||
		    write(ufd, cd.cd_ctfdata, cd.cd_ctflen) != cd.cd_ctflen) {
			warn("failed to write CTF data to '%s'", ufile);
			error |= E_ERROR;
		}

		(void) close(ufd);
	}

	if (elf != NULL)
		(void) elf_end(elf);

	(void) close(fd);
	return (error);
}
