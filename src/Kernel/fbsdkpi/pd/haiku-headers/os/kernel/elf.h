/*
 * Copyright 2002-2019 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ELF_H
#define _ELF_H


#include <BeBuild.h>
#include <ByteOrder.h>
#include <SupportDefs.h>


typedef uint32 Elf32_Addr;
typedef uint16 Elf32_Half;
typedef uint32 Elf32_Off;
typedef int32 Elf32_Sword;
typedef uint32 Elf32_Word;

typedef Elf32_Half Elf32_Versym;

typedef uint64 Elf64_Addr;
typedef uint64 Elf64_Off;
typedef uint16 Elf64_Half;
typedef uint32 Elf64_Word;
typedef int32 Elf64_Sword;
typedef uint64 Elf64_Xword;
typedef int64 Elf64_Sxword;

typedef Elf64_Half Elf64_Versym;

#if B_HAIKU_32_BIT
	#define Elf_Addr Elf32_Addr
	#define Elf_Phdr Elf32_Phdr
	#define Elf_Half Elf32_Half
	#define ElfW(x) Elf32_ ## x
#elif B_HAIKU_64_BIT
	#define Elf_Addr Elf64_Addr
	#define Elf_Phdr Elf64_Phdr
	#define Elf_Half Elf64_Half
	#define ElfW(x) Elf64_ ## x
#endif


/*** ELF header ***/

#define EI_NIDENT	16

typedef struct {
	uint8		e_ident[EI_NIDENT];
	Elf32_Half	e_type;
	Elf32_Half	e_machine;
	Elf32_Word	e_version;
	Elf32_Addr	e_entry;
	Elf32_Off	e_phoff;
	Elf32_Off	e_shoff;
	Elf32_Word	e_flags;
	Elf32_Half	e_ehsize;
	Elf32_Half	e_phentsize;
	Elf32_Half	e_phnum;
	Elf32_Half	e_shentsize;
	Elf32_Half	e_shnum;
	Elf32_Half	e_shstrndx;

#ifdef __cplusplus
	bool IsHostEndian() const;
#endif
} Elf32_Ehdr;

typedef struct {
	uint8		e_ident[EI_NIDENT];
	Elf64_Half	e_type;
	Elf64_Half	e_machine;
	Elf64_Word	e_version;
	Elf64_Addr	e_entry;
	Elf64_Off	e_phoff;
	Elf64_Off	e_shoff;
	Elf64_Word	e_flags;
	Elf64_Half	e_ehsize;
	Elf64_Half	e_phentsize;
	Elf64_Half	e_phnum;
	Elf64_Half	e_shentsize;
	Elf64_Half	e_shnum;
	Elf64_Half	e_shstrndx;

#ifdef __cplusplus
	bool IsHostEndian() const;
#endif
} Elf64_Ehdr;

/* e_ident[] indices */
#define EI_MAG0		0
#define EI_MAG1		1
#define EI_MAG2		2
#define EI_MAG3		3
#define EI_CLASS	4
#define EI_DATA		5
#define EI_VERSION	6
#define EI_PAD		7

/* Values for the magic number bytes. */
#define ELFMAG0		0x7f
#define ELFMAG1		'E'
#define ELFMAG2		'L'
#define ELFMAG3		'F'
#define ELFMAG		"\x7f""ELF"
#define SELFMAG		4

/* e_ident */
#define IS_ELF(ehdr)    ((ehdr).e_ident[EI_MAG0] == ELFMAG0 && \
	(ehdr).e_ident[EI_MAG1] == ELFMAG1 && \
	(ehdr).e_ident[EI_MAG2] == ELFMAG2 && \
	(ehdr).e_ident[EI_MAG3] == ELFMAG3)

/* e_type (Object file type) */
#define ET_NONE			0 /* No file type */
#define ET_REL			1 /* Relocatable file */
#define ET_EXEC			2 /* Executable file */
#define ET_DYN			3 /* Shared-object file */
#define ET_CORE			4 /* Core file */
#define ET_LOOS			0xfe00 /* OS-specific range start */
#define ET_HIOS			0xfeff /* OS-specific range end */
#define ET_LOPROC		0xff00 /* Processor-specific range start */
#define ET_HIPROC		0xffff /* Processor-specific range end */

/* e_machine (Architecture) */
#define EM_NONE			0 /* No machine */
#define EM_M32			1 /* AT&T WE 32100 */
#define EM_SPARC		2 /* Sparc */
#define EM_386			3 /* Intel 80386 */
#define EM_68K			4 /* Motorola m68k family */
#define EM_88K			5 /* Motorola m88k family */
#define EM_486			6 /* Intel 80486, Reserved for future use */
#define EM_860			7 /* Intel 80860 */
#define EM_MIPS			8 /* MIPS R3000 (officially, big-endian only) */
#define EM_S370			9 /* IBM System/370 */
#define EM_MIPS_RS3_LE	10 /* MIPS R3000 little-endian, Deprecated */
#define EM_PARISC		15 /* HPPA */
#define EM_VPP550		17 /* Fujitsu VPP500 */
#define EM_SPARC32PLUS	18 /* Sun "v8plus" */
#define EM_960			19 /* Intel 80960 */
#define EM_PPC			20 /* PowerPC */
#define EM_PPC64		21 /* 64-bit PowerPC */
#define EM_S390			22 /* IBM S/390 */
#define EM_V800			36 /* NEC V800 series */
#define EM_FR20			37 /* Fujitsu FR20 */
#define EM_RH32			38 /* TRW RH32 */
#define EM_MCORE		39 /* Motorola M*Core */
#define EM_RCE			39 /* Old name for MCore */
#define EM_ARM			40 /* ARM */
#define EM_OLD_ALPHA	41 /* Digital Alpha */
#define EM_SH			42 /* Renesas / SuperH SH */
#define EM_SPARCV9		43 /* SPARC v9 64-bit */
#define EM_TRICORE		44 /* Siemens Tricore embedded processor */
#define EM_ARC			45 /* ARC Cores */
#define EM_H8_300		46 /* Renesas H8/300 */
#define EM_H8_300H		47 /* Renesas H8/300H */
#define EM_H8S			48 /* Renesas H8S */
#define EM_H8_500		49 /* Renesas H8/500 */
#define EM_IA_64		50 /* Intel IA-64 Processor */
#define EM_MIPS_X		51 /* Stanford MIPS-X */
#define EM_COLDFIRE		52 /* Motorola Coldfire */
#define EM_68HC12		53 /* Motorola M68HC12 */
#define EM_MMA			54 /* Fujitsu Multimedia Accelerator */
#define EM_PCP			55 /* Siemens PCP */
#define EM_NCPU			56 /* Sony nCPU embedded RISC processor */
#define EM_NDR1			57 /* Denso NDR1 microprocesspr */
#define EM_STARCORE		58 /* Motorola Star*Core processor */
#define EM_ME16			59 /* Toyota ME16 processor */
#define EM_ST100		60 /* STMicroelectronics ST100 processor */
#define EM_TINYJ		61 /* Advanced Logic Corp. TinyJ embedded processor */
#define EM_X86_64		62 /* Advanced Micro Devices X86-64 processor */
#define EM_PDSP			63 /* Sony DSP Processor */
#define EM_FX66			66 /* Siemens FX66 microcontroller */
#define EM_ST9PLUS		67 /* STMicroelectronics ST9+ 8/16 mc */
#define EM_ST7			68 /* STmicroelectronics ST7 8 bit mc */
#define EM_68HC16		69 /* Motorola MC68HC16 microcontroller */
#define EM_68HC11		70 /* Motorola MC68HC11 microcontroller */
#define EM_68HC08		71 /* Motorola MC68HC08 microcontroller */
#define EM_68HC05		72 /* Motorola MC68HC05 microcontroller */
#define EM_SVX			73 /* Silicon Graphics SVx */
#define EM_ST19			74 /* STMicroelectronics ST19 8 bit mc */
#define EM_VAX			75 /* Digital VAX */
#define EM_CRIS			76 /* Axis Communications 32-bit embedded processor */
#define EM_JAVELIN		77 /* Infineon Technologies 32-bit embedded processor */
#define EM_FIREPATH		78 /* Element 14 64-bit DSP Processor */
#define EM_ZSP			79 /* LSI Logic 16-bit DSP Processor */
#define EM_MMIX			80 /* Donald Knuth's educational 64-bit processor */
#define EM_HUANY		81 /* Harvard University machine-independent object files */
#define EM_PRISM		82 /* SiTera Prism */
#define EM_AVR			83 /* Atmel AVR 8-bit microcontroller */
#define EM_FR30			84 /* Fujitsu FR30 */
#define EM_D10V			85 /* Mitsubishi D10V */
#define EM_D30V			86 /* Mitsubishi D30V */
#define EM_V850			87 /* NEC v850 */
#define EM_M32R			88 /* Mitsubishi M32R */
#define EM_MN10300		89 /* Matsushita MN10300 */
#define EM_MN10200		90 /* Matsushita MN10200 */
#define EM_PJ			91 /* picoJava */
#define EM_OPENRISC		92 /* OpenRISC 32-bit embedded processor */
#define EM_ARC_A5		93 /* ARC Cores Tangent-A5 */
#define EM_XTENSA		94 /* Tensilica Xtensa Architecture */
#define EM_VIDCORE		95 /* Alphamosaic VideoCore */
#define EM_CR			103 /* Nat. Semi. CompactRISC microprocessor */
#define EM_BLACKFIN		106 /* Analog Devices Blackfin (DSP) processor */
#define EM_ARCA			109 /* Arca RISC Microprocessor */
#define EM_VIDCORE3		137 /* Broadcom VideoCore III */
#define EM_ARM64		183 /* ARM 64 bit */
#define EM_AARCH64		EM_ARM64
#define EM_AVR32		185 /* AVR-32 */
#define EM_STM8			186 /* ST STM8S */
#define EM_CUDA			190 /* Nvidia CUDA */
#define EM_VIDCORE5		198 /* Broadcom VideoCore V */
#define EM_NORC			218 /* Nanoradio Optimized RISC */
#define EM_AMDGPU		224 /* AMD GPU */
#define EM_RISCV		243 /* RISC-V */
#define EM_BPF			247 /* Linux kernel bpf virtual machine */

#define EM_ALPHA        0x9026 /* Digital Alpha (nonstandard, but the value
								  everyone uses) */

/* architecture class (EI_CLASS) */
#define ELFCLASSNONE	0
#define ELFCLASS32		1
#define ELFCLASS64		2

/* endian (EI_DATA) */
#define ELFDATANONE	0	/* invalid */
#define ELFDATA2LSB	1	/* little endian */
#define ELFDATA2MSB	2	/* big endian */

/* ELF version (EI_VERSION) */
#define EV_NONE		0	/* invalid */
#define EV_CURRENT	1	/* current version */

/*** section header ***/

typedef struct {
	Elf32_Word	sh_name;
	Elf32_Word	sh_type;
	Elf32_Word	sh_flags;
	Elf32_Addr	sh_addr;
	Elf32_Off	sh_offset;
	Elf32_Word	sh_size;
	Elf32_Word	sh_link;
	Elf32_Word	sh_info;
	Elf32_Word	sh_addralign;
	Elf32_Word	sh_entsize;
} Elf32_Shdr;

typedef struct {
	Elf64_Word	sh_name;
	Elf64_Word	sh_type;
	Elf64_Xword	sh_flags;
	Elf64_Addr	sh_addr;
	Elf64_Off	sh_offset;
	Elf64_Xword	sh_size;
	Elf64_Word	sh_link;
	Elf64_Word	sh_info;
	Elf64_Xword	sh_addralign;
	Elf64_Xword	sh_entsize;
} Elf64_Shdr;

/* special section indices */
#define SHN_UNDEF		0
#define SHN_LORESERVE	0xff00
#define SHN_LOPROC		0xff00
#define SHN_HIPROC		0xff1f
#define SHN_ABS			0xfff1
#define SHN_COMMON		0xfff2
#define SHN_HIRESERVE	0xffff

/* section header type */
#define SHT_NULL		0
#define SHT_PROGBITS	1
#define SHT_SYMTAB		2
#define SHT_STRTAB		3
#define SHT_RELA		4
#define SHT_HASH		5
#define SHT_DYNAMIC		6
#define SHT_NOTE		7
#define SHT_NOBITS		8
#define SHT_REL			9
#define SHT_SHLIB		10
#define SHT_DYNSYM		11
#define SHT_INIT_ARRAY	14
#define SHT_FINI_ARRAY	15

#define SHT_GNU_verdef	0x6ffffffd    /* version definition section */
#define SHT_GNU_verneed	0x6ffffffe    /* version needs section */
#define SHT_GNU_versym	0x6fffffff    /* version symbol table */

#define SHT_LOPROC		0x70000000
#define SHT_HIPROC		0x7fffffff
#define SHT_LOUSER		0x80000000
#define SHT_HIUSER		0xffffffff

/* section header flags */
#define SHF_WRITE		1
#define SHF_ALLOC		2
#define SHF_EXECINSTR	4

#define SHF_MASKPROC	0xf0000000


/*** program header ***/

typedef struct {
	Elf32_Word	p_type;
	Elf32_Off	p_offset;	/* offset from the beginning of the file of the segment */
	Elf32_Addr	p_vaddr;	/* virtual address for the segment in memory */
	Elf32_Addr	p_paddr;
	Elf32_Word	p_filesz;	/* the size of the segment in the file */
	Elf32_Word	p_memsz;	/* the size of the segment in memory */
	Elf32_Word	p_flags;
	Elf32_Word	p_align;

#ifdef __cplusplus
	bool IsReadWrite() const;
	bool IsExecutable() const;
#endif
} Elf32_Phdr;

typedef struct {
	Elf64_Word	p_type;
	Elf64_Word	p_flags;
	Elf64_Off	p_offset;	/* offset from the beginning of the file of the segment */
	Elf64_Addr	p_vaddr;	/* virtual address for the segment in memory */
	Elf64_Addr	p_paddr;
	Elf64_Xword	p_filesz;	/* the size of the segment in the file */
	Elf64_Xword	p_memsz;	/* the size of the segment in memory */
	Elf64_Xword	p_align;

#ifdef __cplusplus
	bool IsReadWrite() const;
	bool IsExecutable() const;
#endif
} Elf64_Phdr;

/* program header segment types */
#define PT_NULL			0
#define PT_LOAD			1
#define PT_DYNAMIC		2
#define PT_INTERP		3
#define PT_NOTE			4
#define PT_SHLIB		5
#define PT_PHDR			6
#define PT_TLS			7
#define PT_EH_FRAME		0x6474e550
#define PT_STACK		0x6474e551
#define PT_RELRO		0x6474e552

#define PT_LOPROC		0x70000000
#define PT_ARM_UNWIND	0x70000001
#define PT_RISCV_ATTRIBUTES	0x70000003
#define PT_HIPROC		0x7fffffff

/* program header segment flags */
#define PF_EXECUTE	0x1
#define PF_WRITE	0x2
#define PF_READ		0x4
#define PF_PROTECTION_MASK (PF_EXECUTE | PF_WRITE | PF_READ)

#define PF_MASKPROC	0xf0000000


/* symbol table entry */

typedef struct {
	Elf32_Word	st_name;
	Elf32_Addr	st_value;
	Elf32_Word	st_size;
	uint8		st_info;
	uint8 		st_other;
	Elf32_Half	st_shndx;

#ifdef __cplusplus
	uint8 Bind() const;
	uint8 Type() const;
	void SetInfo(uint8 bind, uint8 type);
#endif
} Elf32_Sym;

typedef struct {
	Elf64_Word	st_name;
	uint8		st_info;
	uint8		st_other;
	Elf64_Half	st_shndx;
	Elf64_Addr	st_value;
	Elf64_Xword	st_size;

#ifdef __cplusplus
	uint8 Bind() const;
	uint8 Type() const;
	void SetInfo(uint8 bind, uint8 type);
#endif
} Elf64_Sym;

#define ELF32_ST_BIND(i) ((i) >> 4)
#define ELF32_ST_TYPE(i) ((i) & 0xf)
#define ELF32_ST_INFO(b, t) (((b) << 4) + ((t) & 0xf))
#define ELF32_ST_VISIBILITY(o) ((o) & 0x3)

#define ELF64_ST_BIND(i) ((i) >> 4)
#define ELF64_ST_TYPE(i) ((i) & 0xf)
#define ELF64_ST_INFO(b, t) (((b) << 4) + ((t) & 0xf))
#define ELF64_ST_VISIBILITY(o) ((o) & 0x3)

/* symbol types */
#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4
#define STT_TLS		6
#define STT_LOPROC 13
#define STT_HIPROC 15

/* symbol binding */
#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2
#define STB_LOPROC 13
#define STB_HIPROC 15

/* symbol visibility */
#define STV_DEFAULT 0
#define STV_INTERNAL 1
#define STV_HIDDEN 2
#define STV_PROTECTED 3

/* special symbol indices */
#define STN_UNDEF 0

/* relocation types */

#define R_386_NONE		0
#define R_386_32		1	/* add symbol value */
#define R_386_PC32		2	/* add PC relative symbol value */
#define R_386_GOT32		3	/* add PC relative GOT offset */
#define R_386_PLT32		4	/* add PC relative PLT offset */
#define R_386_COPY		5	/* copy data from shared object */
#define R_386_GLOB_DAT	6	/* set GOT entry to data address */
#define R_386_JMP_SLOT	7	/* set GOT entry to code address */
#define R_386_RELATIVE	8	/* add load address of shared object */
#define R_386_GOTOFF	9	/* add GOT relative symbol address */
#define R_386_GOTPC		10	/* add PC relative GOT table address */
#define R_386_TLS_DTPMOD32	35
#define R_386_TLS_DTPOFF32	36


/* relocation table entry */

typedef struct {
	Elf32_Addr r_offset;
	Elf32_Word r_info;

#ifdef __cplusplus
	uint8 SymbolIndex() const;
	uint8 Type() const;
#endif
} Elf32_Rel;

typedef struct {
	Elf64_Addr	r_offset;
	Elf64_Xword	r_info;

#ifdef __cplusplus
	uint8 SymbolIndex() const;
	uint8 Type() const;
#endif
} Elf64_Rel;

#ifdef __cplusplus
typedef struct Elf32_Rela : public Elf32_Rel {
#else
typedef struct {
	Elf32_Addr r_offset;
	Elf32_Word r_info;
#endif
	Elf32_Sword r_addend;
} Elf32_Rela;

#ifdef __cplusplus
typedef struct Elf64_Rela : public Elf64_Rel {
#else
typedef struct {
	Elf64_Addr		r_offset;
	Elf64_Xword		r_info;
#endif
	Elf64_Sxword	r_addend;
} Elf64_Rela;

#define ELF32_R_SYM(i) ((i) >> 8)
#define ELF32_R_TYPE(i) ((unsigned char)(i))
#define ELF32_R_INFO(s, t) (((s) << 8) + (unsigned char)(t))

#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffffL)
#define ELF64_R_INFO(s, t) ((((Elf64_Xword)(s)) << 32) + ((t) & 0xffffffffL))


/* dynamic section entry */

typedef struct {
	Elf32_Sword d_tag;
	union {
		Elf32_Word d_val;
		Elf32_Addr d_ptr;
	} d_un;
} Elf32_Dyn;

typedef struct {
	Elf64_Sxword d_tag;
	union {
		Elf64_Xword	d_val;
		Elf64_Addr	d_ptr;
	} d_un;
} Elf64_Dyn;

/* dynamic entry type */
#define DT_NULL				0
#define DT_NEEDED			1
#define DT_PLTRELSZ			2
#define DT_PLTGOT			3
#define DT_HASH				4
#define DT_STRTAB			5
#define DT_SYMTAB			6
#define DT_RELA				7
#define DT_RELASZ			8
#define DT_RELAENT			9
#define DT_STRSZ			10
#define DT_SYMENT			11
#define DT_INIT				12
#define DT_FINI				13
#define DT_SONAME			14
#define DT_RPATH			15
#define DT_SYMBOLIC			16
#define DT_REL				17
#define DT_RELSZ			18
#define DT_RELENT			19
#define DT_PLTREL			20
#define DT_DEBUG			21
#define DT_TEXTREL			22
#define DT_JMPREL			23
#define DT_BIND_NOW			24	/* no lazy binding */
#define DT_INIT_ARRAY		25	/* init function array */
#define DT_FINI_ARRAY		26	/* termination function array */
#define DT_INIT_ARRAYSZ		27	/* init function array size */
#define DT_FINI_ARRAYSZ		28	/* termination function array size */
#define DT_RUNPATH			29	/* library search path (supersedes DT_RPATH) */
#define DT_FLAGS			30	/* flags (see below) */
#define DT_ENCODING			32
#define DT_PREINIT_ARRAY	32	/* preinitialization array */
#define DT_PREINIT_ARRAYSZ	33	/* preinitialization array size */
#define DT_NUM				38	/* size of the machine-independent dynamic section */

#define	DT_GNU_HASH		0x6ffffef5	/* GNU-style hash table */

#define DT_VERSYM       0x6ffffff0	/* symbol version table */
#define DT_VERDEF		0x6ffffffc	/* version definition table */
#define DT_VERDEFNUM	0x6ffffffd	/* number of version definitions */
#define DT_VERNEED		0x6ffffffe 	/* table with needed versions */
#define DT_VERNEEDNUM	0x6fffffff	/* number of needed versions */

#define DT_LOPROC		0x70000000
#define DT_HIPROC		0x7fffffff

/* DT_FLAGS values */
#define DF_ORIGIN		0x01
#define DF_SYMBOLIC		0x02
#define DF_TEXTREL		0x04
#define DF_BIND_NOW		0x08
#define DF_STATIC_TLS	0x10


/* version definition section */

typedef struct {
	Elf32_Half	vd_version;		/* version revision */
	Elf32_Half	vd_flags;		/* version information flags */
	Elf32_Half	vd_ndx;			/* version index as specified in the
								   symbol version table */
	Elf32_Half	vd_cnt;			/* number of associated verdaux entries */
	Elf32_Word	vd_hash;		/* version name hash value */
	Elf32_Word	vd_aux;			/* byte offset to verdaux array */
	Elf32_Word	vd_next;		/* byte offset to next verdef entry */
} Elf32_Verdef;

typedef struct {
	Elf64_Half	vd_version;		/* version revision */
	Elf64_Half	vd_flags;		/* version information flags */
	Elf64_Half	vd_ndx;			/* version index as specified in the
								   symbol version table */
	Elf64_Half	vd_cnt;			/* number of associated verdaux entries */
	Elf64_Word	vd_hash;		/* version name hash value */
	Elf64_Word	vd_aux;			/* byte offset to verdaux array */
	Elf64_Word	vd_next;		/* byte offset to next verdef entry */
} Elf64_Verdef;

/* values for vd_version (version revision) */
#define VER_DEF_NONE		0		/* no version */
#define VER_DEF_CURRENT		1		/* current version */
#define VER_DEF_NUM			2		/* given version number */

/* values for vd_flags (version information flags) */
#define VER_FLG_BASE		0x1		/* version definition of file itself */
#define VER_FLG_WEAK		0x2 	/* weak version identifier */

/* values for versym symbol index */
#define VER_NDX_LOCAL		0		/* symbol is local */
#define VER_NDX_GLOBAL		1		/* symbol is global/unversioned */
#define VER_NDX_INITIAL		2		/* initial version -- that's the one given
									   to symbols when a library becomes
									   versioned; handled by the linker (and
									   runtime loader) similar to
									   VER_NDX_GLOBAL */
#define VER_NDX_LORESERVE	0xff00	/* beginning of reserved entries */
#define VER_NDX_ELIMINATE	0xff01	/* symbol is to be eliminated */

#define VER_NDX_FLAG_HIDDEN	0x8000	/* flag: version is hidden */
#define VER_NDX_MASK		0x7fff	/* mask to get the actual version index */
#define VER_NDX(x)			((x) & VER_NDX_MASK)


/* auxiliary version information */

typedef struct {
	Elf32_Word	vda_name;		/* string table offset to version or dependency
								   name */
	Elf32_Word	vda_next;		/* byte offset to next verdaux entry */
} Elf32_Verdaux;

typedef struct {
	Elf64_Word	vda_name;		/* string table offset to version or dependency
								   name */
	Elf64_Word	vda_next;		/* byte offset to next verdaux entry */
} Elf64_Verdaux;


/* version dependency section */

typedef struct {
	Elf32_Half	vn_version;		/* version of structure */
	Elf32_Half	vn_cnt;			/* number of associated vernaux entries */
	Elf32_Word	vn_file;		/* byte offset to file name for this
								   dependency */
	Elf32_Word	vn_aux;			/* byte offset to vernaux array */
	Elf32_Word	vn_next;		/* byte offset to next verneed entry */
} Elf32_Verneed;

typedef struct {
	Elf64_Half	vn_version;		/* version of structure */
	Elf64_Half	vn_cnt;			/* number of associated vernaux entries */
	Elf64_Word	vn_file;		/* byte offset to file name for this
								   dependency */
	Elf64_Word	vn_aux;			/* byte offset to vernaux array */
	Elf64_Word	vn_next;		/* byte offset to next verneed entry */
} Elf64_Verneed;

/* values for vn_version (version revision) */
#define VER_NEED_NONE		0	/* no version */
#define VER_NEED_CURRENT	1	/* current version */
#define VER_NEED_NUM		2	/* given version number */


/* auxiliary needed version information */

typedef struct {
	Elf32_Word	vna_hash;		/* dependency name hash value */
	Elf32_Half	vna_flags;		/* dependency specific information flags */
	Elf32_Half	vna_other;		/* version index as specified in the symbol
								   version table */
	Elf32_Word	vna_name;		/* string table offset to dependency name */
	Elf32_Word	vna_next;		/* byte offset to next vernaux entry */
} Elf32_Vernaux;

typedef struct {
	Elf64_Word	vna_hash;		/* dependency name hash value */
	Elf64_Half	vna_flags;		/* dependency specific information flags */
	Elf64_Half	vna_other;		/* version index as specified in the symbol
								   version table */
	Elf64_Word	vna_name;		/* string table offset to dependency name */
	Elf64_Word	vna_next;		/* byte offset to next vernaux entry */
} Elf64_Vernaux;

/* values for vna_flags */
#define VER_FLG_WEAK	0x2		/* weak version identifier */


/*** core files ***/

/* note section header */

typedef struct {
	Elf32_Word n_namesz;		/* length of the note's name */
	Elf32_Word n_descsz;		/* length of the note's descriptor */
	Elf32_Word n_type;			/* note type */
} Elf32_Nhdr;

typedef struct {
	Elf64_Word n_namesz;		/* length of the note's name */
	Elf64_Word n_descsz;		/* length of the note's descriptor */
	Elf64_Word n_type;			/* note type */
} Elf64_Nhdr;

/* values for note name */
#define ELF_NOTE_CORE		"CORE"
#define ELF_NOTE_HAIKU		"Haiku"

/* values for note type (n_type) */
/* ELF_NOTE_CORE/... */
#define NT_FILE				0x46494c45 /* mapped files */

/* ELF_NOTE_HAIKU/... */
#define NT_TEAM				0x7465616d 	/* team */
#define NT_AREAS			0x61726561 	/* areas */
#define NT_IMAGES			0x696d6167 	/* images */
#define NT_THREADS			0x74687264 	/* threads */
#define NT_SYMBOLS			0x73796d73 	/* symbols */

#define NT_GNU_BUILD_ID		3

/* NT_TEAM: uint32 entrySize; Elf32_Note_Team; char[] args */
typedef struct {
	int32		nt_id;				/* team ID */
	int32		nt_uid;				/* team owner ID */
	int32		nt_gid;				/* team group ID */
} Elf32_Note_Team;

typedef Elf32_Note_Team Elf64_Note_Team;

/* NT_AREAS:
 * uint32 count;
 * uint32 entrySize;
 * Elf32_Note_Area_Entry[count];
 * char[] names
 */
typedef struct {
	int32		na_id;				/* area ID */
	uint32		na_lock;			/* lock type (B_NO_LOCK, ...) */
	uint32		na_protection;		/* protection (B_READ_AREA | ...) */
	uint32		na_base;			/* area base address */
	uint32		na_size;			/* area size */
	uint32		na_ram_size;		/* physical memory used */
} Elf32_Note_Area_Entry;

/* NT_AREAS:
 * uint32 count;
 * uint32 entrySize;
 * Elf64_Note_Area_Entry[count];
 * char[] names
 */
typedef struct {
	int32		na_id;				/* area ID */
	uint32		na_lock;			/* lock type (B_NO_LOCK, ...) */
	uint32		na_protection;		/* protection (B_READ_AREA | ...) */
	uint32		na_pad1;
	uint64		na_base;			/* area base address */
	uint64		na_size;			/* area size */
	uint64		na_ram_size;		/* physical memory used */
} Elf64_Note_Area_Entry;

/* NT_IMAGES:
 * uint32 count;
 * uint32 entrySize;
 * Elf32_Note_Image_Entry[count];
 * char[] names
 */
typedef struct {
	int32		ni_id;				/* image ID */
	int32		ni_type;			/* image type (B_APP_IMAGE, ...) */
	uint32		ni_init_routine;	/* address of init function */
	uint32		ni_term_routine;	/* address of termination function */
	int32		ni_device;			/* device ID of mapped file */
	int64		ni_node;			/* node ID of mapped file */
	uint32		ni_text_base;		/* base address of text segment */
	uint32		ni_text_size;		/* size of text segment */
	int32		ni_text_delta;		/* delta of the text segment relative to
									   load address specified in the ELF file */
	uint32		ni_data_base;		/* base address of data segment */
	uint32		ni_data_size;		/* size of data segment */
	uint32		ni_symbol_table;	/* address of dynamic symbol table */
	uint32		ni_symbol_hash;		/* address of dynamic symbol hash */
	uint32		ni_string_table;	/* address of dynamic string table */
} Elf32_Note_Image_Entry;

/* NT_IMAGES:
 * uint32 count;
 * uint32 entrySize;
 * Elf64_Note_Image_Entry[count];
 * char[] names
 */
typedef struct {
	int32		ni_id;				/* image ID */
	int32		ni_type;			/* image type (B_APP_IMAGE, ...) */
	uint64		ni_init_routine;	/* address of init function */
	uint64		ni_term_routine;	/* address of termination function */
	uint32		ni_pad1;
	int32		ni_device;			/* device ID of mapped file */
	int64		ni_node;			/* node ID of mapped file */
	uint64		ni_text_base;		/* base address of text segment */
	uint64		ni_text_size;		/* size of text segment */
	int64		ni_text_delta;		/* delta of the text segment relative to
									   load address specified in the ELF file */
	uint64		ni_data_base;		/* base address of data segment */
	uint64		ni_data_size;		/* size of data segment */
	uint64		ni_symbol_table;	/* address of dynamic symbol table */
	uint64		ni_symbol_hash;		/* address of dynamic symbol hash */
	uint64		ni_string_table;	/* address of dynamic string table */
} Elf64_Note_Image_Entry;

/* NT_THREADS:
 * uint32 count;
 * uint32 entrySize;
 * uint32 cpuStateSize;
 * {Elf32_Note_Thread_Entry, uint8[cpuStateSize] cpuState}[count];
 * char[] names
 */
typedef struct {
	int32		nth_id;				/* thread ID */
	int32		nth_state;			/* thread state (B_THREAD_RUNNING, ...) */
	int32		nth_priority;		/* thread priority */
	uint32		nth_stack_base;		/* thread stack base address */
	uint32		nth_stack_end;		/* thread stack end address */
} Elf32_Note_Thread_Entry;

/* NT_THREADS:
 * uint32 count;
 * uint32 entrySize;
 * uint32 cpuStateSize;
 * {Elf64_Note_Thread_Entry, uint8[cpuStateSize] cpuState}[count];
 * char[] names
 */
typedef struct {
	int32		nth_id;				/* thread ID */
	int32		nth_state;			/* thread state (B_THREAD_RUNNING, ...) */
	int32		nth_priority;		/* thread priority */
	uint32		nth_pad1;
	uint64		nth_stack_base;		/* thread stack base address */
	uint64		nth_stack_end;		/* thread stack end address */
} Elf64_Note_Thread_Entry;

/* NT_SYMBOLS:
 * int32 imageId;
 * uint32 symbolCount;
 * uint32 entrySize;
 * Elf{32,64}_Sym[count];
 * char[] strings
 */


/*** inline functions ***/

#ifdef __cplusplus

inline bool
Elf32_Ehdr::IsHostEndian() const
{
#if B_HOST_IS_LENDIAN
	return e_ident[EI_DATA] == ELFDATA2LSB;
#elif B_HOST_IS_BENDIAN
	return e_ident[EI_DATA] == ELFDATA2MSB;
#endif
}


inline bool
Elf64_Ehdr::IsHostEndian() const
{
#if B_HOST_IS_LENDIAN
	return e_ident[EI_DATA] == ELFDATA2LSB;
#elif B_HOST_IS_BENDIAN
	return e_ident[EI_DATA] == ELFDATA2MSB;
#endif
}


inline bool
Elf32_Phdr::IsReadWrite() const
{
	return !(~p_flags & (PF_READ | PF_WRITE));
}


inline bool
Elf32_Phdr::IsExecutable() const
{
	return (p_flags & PF_EXECUTE) != 0;
}


inline bool
Elf64_Phdr::IsReadWrite() const
{
	return !(~p_flags & (PF_READ | PF_WRITE));
}


inline bool
Elf64_Phdr::IsExecutable() const
{
	return (p_flags & PF_EXECUTE) != 0;
}


inline uint8
Elf32_Sym::Bind() const
{
	return ELF32_ST_BIND(st_info);
}


inline uint8
Elf32_Sym::Type() const
{
	return ELF32_ST_TYPE(st_info);
}


inline void
Elf32_Sym::SetInfo(uint8 bind, uint8 type)
{
	st_info = ELF32_ST_INFO(bind, type);
}


inline uint8
Elf64_Sym::Bind() const
{
	return ELF64_ST_BIND(st_info);
}


inline uint8
Elf64_Sym::Type() const
{
	return ELF64_ST_TYPE(st_info);
}


inline void
Elf64_Sym::SetInfo(uint8 bind, uint8 type)
{
	st_info = ELF64_ST_INFO(bind, type);
}


inline uint8
Elf32_Rel::SymbolIndex() const
{
	return ELF32_R_SYM(r_info);
}


inline uint8
Elf32_Rel::Type() const
{
	return ELF32_R_TYPE(r_info);
}


inline uint8
Elf64_Rel::SymbolIndex() const
{
	return ELF64_R_SYM(r_info);
}


inline uint8
Elf64_Rel::Type() const
{
	return ELF64_R_TYPE(r_info);
}

#endif	/* __cplusplus */


#endif	/* _ELF_H */
