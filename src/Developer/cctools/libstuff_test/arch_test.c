//
//  arch_test.c
//  libstuff_test
//
//  Created by Michael Trent on 1/20/19.
//

#include "test_main.h"

#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>

#include "stuff/bool.h"
#include "stuff/arch.h"

static void check_arch_flag(const char* name, cpu_type_t cputype,
                            cpu_subtype_t cpusubtype)
{
  struct arch_flag af;
  
  get_arch_from_flag((char*)name, &af);
  check_set_prefix("arch '%s'", name);
  check_uint32("cputype", af.cputype, cputype);
  check_uint32("cpusubtype", af.cpusubtype, cpusubtype);
}

static void check_arch_name(const char* name, cpu_type_t cputype,
                            cpu_subtype_t cpusubtype)
{
  const char* nm = get_arch_name_from_types(cputype, cpusubtype);
  check_set_prefix("arch (%u, %u)", cputype, cpusubtype);
  check_string("name", name, nm);
}

static void check_arch_if_known(const char* name, cpu_type_t cputype,
                                  cpu_subtype_t cpusubtype)
{
  const char* nm = get_arch_name_if_known(cputype, cpusubtype);
  check_set_prefix("arch (%u, %u)", cputype, cpusubtype);
  check_string("name", name, nm);
}

static void check_arch_family(const char* name, cpu_type_t cputype)
{
  const struct arch_flag *af;
  
  af = get_arch_family_from_cputype(cputype);
  if (af) {
    check_string("name", name, af->name);
  } else if (name) {
    test_printerr("arch family not found for cputype %u: %s", cputype);
  }
}

jmp_buf env;

static void sig_handler(int signo)
{
  longjmp(env, 1);
}

static void check_arch_values(const char* name, enum byte_sex byteorder,
                              uint64_t stack_addr, uint32_t seg_align,
                              int abort)
{
  // so ... silly me ... get_byte_sex_from_flag will abort on a bad cpu type
  // so we know right away that code is doing something bad. So let's trap
  // the abort.
  //
  // you need to hand this to lldb every. single. time.
  //
  // process handle SIGABRT -n true -p true -s false
  
  if (0 == setjmp(env)) {
    
    void (*ohandler)(int);
    ohandler = signal(SIGABRT, sig_handler);
    if (ohandler == SIG_ERR) {
      test_printerr("internal error: cannot install SIGABRT handler");
      test_abort();
      return;
    }
    
    struct arch_flag af;
    enum byte_sex bo = UNKNOWN_BYTE_SEX;
    uint64_t sad = 0;
    uint32_t sal = 0;
    
    if (get_arch_from_flag((char*)name, &af)) {
      bo = get_byte_sex_from_flag(&af);
      sad = get_stack_addr_from_flag(&af);
      sal = get_segalign_from_flag(&af);
    }
    
    check_set_prefix("arch '%s'", name);
    check_uint32("byte order", byteorder, bo);
    check_uint64("stack addr", stack_addr, sad);
    check_uint32("segalign", seg_align, sal);
    
    if (signal(SIGABRT, ohandler) == SIG_ERR) {
      test_printerr("internal error: cannot restore SIGABRT handler");
      test_abort();
      return;
    }
  }
  else {
    if (0 == abort)
      test_printerr("arch '%s' unexpectedly aborted", name);
  }
}

static void test_get_arch_from_flag(void)
{
  check_arch_flag("any", CPU_TYPE_ANY, CPU_SUBTYPE_MULTIPLE);
  check_arch_flag("little", CPU_TYPE_ANY, CPU_SUBTYPE_LITTLE_ENDIAN);
  check_arch_flag("big", CPU_TYPE_ANY, CPU_SUBTYPE_BIG_ENDIAN);
  check_arch_flag("ppc64", CPU_TYPE_POWERPC64, CPU_SUBTYPE_POWERPC_ALL);
  check_arch_flag("x86_64", CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_ALL);
  check_arch_flag("x86_64h", CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_H);
  check_arch_flag("ppc970-64", CPU_TYPE_POWERPC64, CPU_SUBTYPE_POWERPC_970);
  check_arch_flag("arm64_32", CPU_TYPE_ARM64_32, CPU_SUBTYPE_ARM64_32_V8);
  check_arch_flag("arm64e", CPU_TYPE_ARM64, CPU_SUBTYPE_ARM64E);
  check_arch_flag("ppc",    CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_ALL);
  check_arch_flag("i386",   CPU_TYPE_I386,    CPU_SUBTYPE_I386_ALL);
  check_arch_flag("m68k",   CPU_TYPE_MC680x0, CPU_SUBTYPE_MC680x0_ALL);
  check_arch_flag("hppa",   CPU_TYPE_HPPA,    CPU_SUBTYPE_HPPA_ALL);
  check_arch_flag("sparc",  CPU_TYPE_SPARC,   CPU_SUBTYPE_SPARC_ALL);
  check_arch_flag("m88k",   CPU_TYPE_MC88000, CPU_SUBTYPE_MC88000_ALL);
  check_arch_flag("i860",   CPU_TYPE_I860,    CPU_SUBTYPE_I860_ALL);
  check_arch_flag("veo",    CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_ALL);
  check_arch_flag("arm",    CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_ALL);
  check_arch_flag("ppc601", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_601);
  check_arch_flag("ppc603", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_603);
  check_arch_flag("ppc603e",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_603e);
  check_arch_flag("ppc603ev",CPU_TYPE_POWERPC,CPU_SUBTYPE_POWERPC_603ev);
  check_arch_flag("ppc604", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_604);
  check_arch_flag("ppc604e",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_604e);
  check_arch_flag("ppc750", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_750);
  check_arch_flag("ppc7400",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_7400);
  check_arch_flag("ppc7450",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_7450);
  check_arch_flag("ppc970", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_970);
  check_arch_flag("i486",   CPU_TYPE_I386,    CPU_SUBTYPE_486);
  check_arch_flag("i486SX", CPU_TYPE_I386,    CPU_SUBTYPE_486SX);
  check_arch_flag("pentium",CPU_TYPE_I386,    CPU_SUBTYPE_PENT);
  check_arch_flag("i586",   CPU_TYPE_I386,    CPU_SUBTYPE_586);
  check_arch_flag("pentpro", CPU_TYPE_I386, CPU_SUBTYPE_PENTPRO);
  check_arch_flag("i686",   CPU_TYPE_I386, CPU_SUBTYPE_PENTPRO);
  check_arch_flag("pentIIm3",CPU_TYPE_I386, CPU_SUBTYPE_PENTII_M3);
  check_arch_flag("pentIIm5",CPU_TYPE_I386, CPU_SUBTYPE_PENTII_M5);
  check_arch_flag("pentium4",CPU_TYPE_I386, CPU_SUBTYPE_PENTIUM_4);
  check_arch_flag("m68030", CPU_TYPE_MC680x0, CPU_SUBTYPE_MC68030_ONLY);
  check_arch_flag("m68040", CPU_TYPE_MC680x0, CPU_SUBTYPE_MC68040);
  check_arch_flag("hppa7100LC", CPU_TYPE_HPPA,  CPU_SUBTYPE_HPPA_7100LC);
  check_arch_flag("veo1",   CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_1);
  check_arch_flag("veo2",   CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_2);
  check_arch_flag("veo3",   CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_3);
  check_arch_flag("veo4",   CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_4);
  check_arch_flag("armv4t", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V4T);
  check_arch_flag("armv5",  CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V5TEJ);
  check_arch_flag("xscale", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_XSCALE);
  check_arch_flag("armv6",  CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V6);
  check_arch_flag("armv6m", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V6M);
  check_arch_flag("armv7",  CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7);
  check_arch_flag("armv7f", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7F);
  check_arch_flag("armv7s", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7S);
  check_arch_flag("armv7k", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7K);
  check_arch_flag("armv7m", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7M);
  check_arch_flag("armv7em", CPU_TYPE_ARM,    CPU_SUBTYPE_ARM_V7EM);
  check_arch_flag("arm64v8", CPU_TYPE_ARM64,   CPU_SUBTYPE_ARM64_V8);
  check_arch_flag("unknown", 0, 0);
}

static void test_get_arch_name_from_types(void)
{
  check_arch_name("any", CPU_TYPE_ANY, CPU_SUBTYPE_MULTIPLE);
  check_arch_name("little", CPU_TYPE_ANY, CPU_SUBTYPE_LITTLE_ENDIAN);
  check_arch_name("big", CPU_TYPE_ANY, CPU_SUBTYPE_BIG_ENDIAN);
  check_arch_name("ppc64", CPU_TYPE_POWERPC64, CPU_SUBTYPE_POWERPC_ALL);
  check_arch_name("x86_64", CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_ALL);
  check_arch_name("x86_64h", CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_H);
  check_arch_name("ppc970-64", CPU_TYPE_POWERPC64, CPU_SUBTYPE_POWERPC_970);
  check_arch_name("arm64_32", CPU_TYPE_ARM64_32, CPU_SUBTYPE_ARM64_32_V8);
  check_arch_name("arm64e", CPU_TYPE_ARM64, CPU_SUBTYPE_ARM64E);
  check_arch_name("ppc",    CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_ALL);
  check_arch_name("i386",   CPU_TYPE_I386,    CPU_SUBTYPE_I386_ALL);
  check_arch_name("m68k",   CPU_TYPE_MC680x0, CPU_SUBTYPE_MC680x0_ALL);
  check_arch_name("hppa",   CPU_TYPE_HPPA,    CPU_SUBTYPE_HPPA_ALL);
  check_arch_name("sparc",  CPU_TYPE_SPARC,   CPU_SUBTYPE_SPARC_ALL);
  check_arch_name("m88k",   CPU_TYPE_MC88000, CPU_SUBTYPE_MC88000_ALL);
  check_arch_name("i860",   CPU_TYPE_I860,    CPU_SUBTYPE_I860_ALL);
  check_arch_name("veo",    CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_ALL);
  check_arch_name("arm",    CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_ALL);
  check_arch_name("ppc601", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_601);
  check_arch_name("ppc603", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_603);
  check_arch_name("ppc603e",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_603e);
  check_arch_name("ppc603ev",CPU_TYPE_POWERPC,CPU_SUBTYPE_POWERPC_603ev);
  check_arch_name("ppc604", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_604);
  check_arch_name("ppc604e",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_604e);
  check_arch_name("ppc750", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_750);
  check_arch_name("ppc7400",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_7400);
  check_arch_name("ppc7450",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_7450);
  check_arch_name("ppc970", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_970);
  check_arch_name("i486",   CPU_TYPE_I386,    CPU_SUBTYPE_486);
  check_arch_name("i486SX", CPU_TYPE_I386,    CPU_SUBTYPE_486SX);
  check_arch_name("pentium",CPU_TYPE_I386,    CPU_SUBTYPE_PENT);
  // no reverse lookup for i586, looks like pentium.
  //check_arch_name("i586",   CPU_TYPE_I386,    CPU_SUBTYPE_586);
  check_arch_name("pentpro", CPU_TYPE_I386, CPU_SUBTYPE_PENTPRO);
  // no reverse lookup for i686, looks like pentpro.
  //check_arch_name("i686",   CPU_TYPE_I386, CPU_SUBTYPE_PENTPRO);
  check_arch_name("pentIIm3",CPU_TYPE_I386, CPU_SUBTYPE_PENTII_M3);
  check_arch_name("pentIIm5",CPU_TYPE_I386, CPU_SUBTYPE_PENTII_M5);
  check_arch_name("pentium4",CPU_TYPE_I386, CPU_SUBTYPE_PENTIUM_4);
  check_arch_name("m68030", CPU_TYPE_MC680x0, CPU_SUBTYPE_MC68030_ONLY);
  check_arch_name("m68040", CPU_TYPE_MC680x0, CPU_SUBTYPE_MC68040);
  check_arch_name("hppa7100LC", CPU_TYPE_HPPA,  CPU_SUBTYPE_HPPA_7100LC);
  check_arch_name("veo1",   CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_1);
  // no reverse lookup for veo2, looks like veo.
  //check_arch_name("veo2",   CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_2);
  check_arch_name("veo3",   CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_3);
  check_arch_name("veo4",   CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_4);
  check_arch_name("armv4t", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V4T);
  check_arch_name("armv5",  CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V5TEJ);
  check_arch_name("xscale", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_XSCALE);
  check_arch_name("armv6",  CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V6);
  check_arch_name("armv6m", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V6M);
  check_arch_name("armv7",  CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7);
  check_arch_name("armv7f", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7F);
  check_arch_name("armv7s", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7S);
  check_arch_name("armv7k", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7K);
  check_arch_name("armv7m", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7M);
  check_arch_name("armv7em", CPU_TYPE_ARM,    CPU_SUBTYPE_ARM_V7EM);
  check_arch_name("arm64v8", CPU_TYPE_ARM64,   CPU_SUBTYPE_ARM64_V8);
  check_arch_name("cputype 0 cpusubtype 0", 0, 0);
}

static void test_get_arch_name_if_known(void)
{
  check_arch_if_known("any", CPU_TYPE_ANY, CPU_SUBTYPE_MULTIPLE);
  check_arch_if_known("little", CPU_TYPE_ANY, CPU_SUBTYPE_LITTLE_ENDIAN);
  check_arch_if_known("big", CPU_TYPE_ANY, CPU_SUBTYPE_BIG_ENDIAN);
  check_arch_if_known("ppc64", CPU_TYPE_POWERPC64, CPU_SUBTYPE_POWERPC_ALL);
  check_arch_if_known("x86_64", CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_ALL);
  check_arch_if_known("x86_64h", CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_H);
  check_arch_if_known("ppc970-64", CPU_TYPE_POWERPC64, CPU_SUBTYPE_POWERPC_970);
  check_arch_if_known("arm64_32", CPU_TYPE_ARM64_32, CPU_SUBTYPE_ARM64_32_V8);
  check_arch_if_known("arm64e", CPU_TYPE_ARM64, CPU_SUBTYPE_ARM64E);
  check_arch_if_known("ppc",    CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_ALL);
  check_arch_if_known("i386",   CPU_TYPE_I386,    CPU_SUBTYPE_I386_ALL);
  check_arch_if_known("m68k",   CPU_TYPE_MC680x0, CPU_SUBTYPE_MC680x0_ALL);
  check_arch_if_known("hppa",   CPU_TYPE_HPPA,    CPU_SUBTYPE_HPPA_ALL);
  check_arch_if_known("sparc",  CPU_TYPE_SPARC,   CPU_SUBTYPE_SPARC_ALL);
  check_arch_if_known("m88k",   CPU_TYPE_MC88000, CPU_SUBTYPE_MC88000_ALL);
  check_arch_if_known("i860",   CPU_TYPE_I860,    CPU_SUBTYPE_I860_ALL);
  check_arch_if_known("veo",    CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_ALL);
  check_arch_if_known("arm",    CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_ALL);
  check_arch_if_known("ppc601", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_601);
  check_arch_if_known("ppc603", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_603);
  check_arch_if_known("ppc603e",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_603e);
  check_arch_if_known("ppc603ev",CPU_TYPE_POWERPC,CPU_SUBTYPE_POWERPC_603ev);
  check_arch_if_known("ppc604", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_604);
  check_arch_if_known("ppc604e",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_604e);
  check_arch_if_known("ppc750", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_750);
  check_arch_if_known("ppc7400",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_7400);
  check_arch_if_known("ppc7450",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_7450);
  check_arch_if_known("ppc970", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_970);
  check_arch_if_known("i486",   CPU_TYPE_I386,    CPU_SUBTYPE_486);
  check_arch_if_known("i486SX", CPU_TYPE_I386,    CPU_SUBTYPE_486SX);
  check_arch_if_known("pentium",CPU_TYPE_I386,    CPU_SUBTYPE_PENT);
  // no reverse lookup for i586, looks like pentium.
  //check_arch_name_known("i586",   CPU_TYPE_I386,    CPU_SUBTYPE_586);
  check_arch_if_known("pentpro", CPU_TYPE_I386, CPU_SUBTYPE_PENTPRO);
  // no reverse lookup for i686, looks like pentpro.
  //check_arch_name_known("i686",   CPU_TYPE_I386, CPU_SUBTYPE_PENTPRO);
  check_arch_if_known("pentIIm3",CPU_TYPE_I386, CPU_SUBTYPE_PENTII_M3);
  check_arch_if_known("pentIIm5",CPU_TYPE_I386, CPU_SUBTYPE_PENTII_M5);
  check_arch_if_known("pentium4",CPU_TYPE_I386, CPU_SUBTYPE_PENTIUM_4);
  check_arch_if_known("m68030", CPU_TYPE_MC680x0, CPU_SUBTYPE_MC68030_ONLY);
  check_arch_if_known("m68040", CPU_TYPE_MC680x0, CPU_SUBTYPE_MC68040);
  check_arch_if_known("hppa7100LC", CPU_TYPE_HPPA,  CPU_SUBTYPE_HPPA_7100LC);
  check_arch_if_known("veo1",   CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_1);
  // no reverse lookup for veo2, looks like veo.
  //check_arch_name_known("veo2",   CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_2);
  check_arch_if_known("veo3",   CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_3);
  check_arch_if_known("veo4",   CPU_TYPE_VEO,     CPU_SUBTYPE_VEO_4);
  check_arch_if_known("armv4t", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V4T);
  check_arch_if_known("armv5",  CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V5TEJ);
  check_arch_if_known("xscale", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_XSCALE);
  check_arch_if_known("armv6",  CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V6);
  check_arch_if_known("armv6m", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V6M);
  check_arch_if_known("armv7",  CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7);
  check_arch_if_known("armv7f", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7F);
  check_arch_if_known("armv7s", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7S);
  check_arch_if_known("armv7k", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7K);
  check_arch_if_known("armv7m", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7M);
  check_arch_if_known("armv7em", CPU_TYPE_ARM,    CPU_SUBTYPE_ARM_V7EM);
  check_arch_if_known("arm64v8", CPU_TYPE_ARM64,   CPU_SUBTYPE_ARM64_V8);
  check_arch_if_known(NULL, 0, 0);
}

static void test_get_arch_family_from_cputype(void)
{
  check_arch_family("any", CPU_TYPE_ANY);
  check_arch_family("ppc64", CPU_TYPE_POWERPC64);
  check_arch_family("x86_64", CPU_TYPE_X86_64);
  check_arch_family("arm64_32", CPU_TYPE_ARM64_32);
  check_arch_family("ppc",    CPU_TYPE_POWERPC);
  check_arch_family("i386",   CPU_TYPE_I386);
  check_arch_family("m68k",   CPU_TYPE_MC680x0);
  check_arch_family("hppa",   CPU_TYPE_HPPA);
  check_arch_family("sparc",  CPU_TYPE_SPARC);
  check_arch_family("m88k",   CPU_TYPE_MC88000);
  check_arch_family("i860",   CPU_TYPE_I860);
  check_arch_family("veo",    CPU_TYPE_VEO);
  check_arch_family("arm",    CPU_TYPE_ARM);
  check_arch_family(NULL, 0);
  check_arch_family(NULL, 2112);

  // there is no family name for ARM64
  check_arch_family(NULL, CPU_TYPE_ARM64);
}

static void test_arch_flag_values(void)
{
  // set a breakpoint at the next line of code. at that break point, type this:
  //   process handle SIGABRT -n true -p true -s false
  // see above.
  check_arch_values("any", UNKNOWN_BYTE_SEX, 0, 0, 1);
  check_arch_values("little", UNKNOWN_BYTE_SEX, 0, 0, 1);
  check_arch_values("big", UNKNOWN_BYTE_SEX, 0, 0, 1);
  check_arch_values("ppc64", BIG_ENDIAN_BYTE_SEX, 0x7ffff00000000, 0x1000, 0);
  check_arch_values("x86_64", LITTLE_ENDIAN_BYTE_SEX, 0x7fff5fc00000, 0x1000,0);
  check_arch_values("x86_64h", LITTLE_ENDIAN_BYTE_SEX, 0x7fff5fc00000,0x1000,0);
  check_arch_values("ppc970-64", BIG_ENDIAN_BYTE_SEX, 0x7ffff00000000,0x1000,0);
  check_arch_values("arm64_32", LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("arm64e", LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("ppc",    BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("i386",   LITTLE_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("m68k",   BIG_ENDIAN_BYTE_SEX, 0x04000000, 0x2000, 0);
  check_arch_values("hppa",   BIG_ENDIAN_BYTE_SEX, 0, 0x2000, 0);
  check_arch_values("sparc",  BIG_ENDIAN_BYTE_SEX, 0xf0000000, 0x2000, 0);
  check_arch_values("m88k",   BIG_ENDIAN_BYTE_SEX, 0xffffe000, 0x2000, 0);
  check_arch_values("i860",   BIG_ENDIAN_BYTE_SEX, 0, 0x2000, 0);
  check_arch_values("veo",    BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("arm",    LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("ppc601", BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("ppc603", BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("ppc603e",BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("ppc603ev",BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("ppc604", BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("ppc604e",BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("ppc750", BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("ppc7400",BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("ppc7450",BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("ppc970", BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("i486",   LITTLE_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("i486SX", LITTLE_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("pentium",LITTLE_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("i586",   LITTLE_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("pentpro", LITTLE_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("i686",   LITTLE_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("pentIIm3",LITTLE_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("pentIIm5",LITTLE_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("pentium4",LITTLE_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("m68030", BIG_ENDIAN_BYTE_SEX, 0x04000000, 0x2000, 0);
  check_arch_values("m68040", BIG_ENDIAN_BYTE_SEX, 0x04000000, 0x2000, 0);
  check_arch_values("hppa7100LC", BIG_ENDIAN_BYTE_SEX, 0, 0x2000, 0);
  check_arch_values("veo1",   BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("veo2",   BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("veo3",   BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("veo4",   BIG_ENDIAN_BYTE_SEX, 0xc0000000, 0x1000, 0);
  check_arch_values("armv4t", LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("armv5",  LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("xscale", LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("armv6",  LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("armv6m", LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("armv7",  LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("armv7f", LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("armv7s", LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("armv7k", LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("armv7m", LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("armv7em", LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("arm64v8", LITTLE_ENDIAN_BYTE_SEX, 0, 0x4000, 0);
  check_arch_values("unknown", UNKNOWN_BYTE_SEX, 0, 0, 0);
  //  check_byte_order(NULL, UNKNOWN_BYTE_SEX, YYY, YYY, 0); .. will crash
}

static void test_shared_region_size(void)
{
  const struct arch_flag* flag = get_arch_flags();
  while (flag->name) {
    uint32_t size = flag->cputype == CPU_TYPE_ARM ? 0x08000000 : 0x10000000;
    uint32_t sz = get_shared_region_size_from_flag(flag);
    check_set_prefix("arch '%s'", flag->name);
    check_uint32("shared region size", size, sz);
    flag++;
  }
}

static void test_force_cpusubtypes(void)
{
  const struct arch_flag* flag = get_arch_flags();
  while (flag->name) {
    enum bool force = flag->cputype == CPU_TYPE_I386 ? TRUE : FALSE;
    enum bool frc = force_cpusubtype_ALL_for_cputype(flag->cputype);
    check_set_prefix("arch '%s'", flag->name);
    check_uint32("shared region size", force, frc);
    flag++;
  }
}

static int test_main(void)
{
  // set a breakpoint at the next line of code. at that break point, type this:
  //   process handle SIGABRT -n true -p true -s false
  // see above.
  int err = 0;
  
  if (!err) err = test_add("test get_arch_from_flag", test_get_arch_from_flag);
  if (!err) err = test_add("test get_arch_name_from_types",
                           test_get_arch_name_from_types);
  if (!err) err = test_add("test get_arch_name_if_known",
                           test_get_arch_name_if_known);
  if (!err) err = test_add("test get_arch_family_from_cputype",
                           test_get_arch_family_from_cputype);
  if (!err) err = test_add("test arch_flag values",
                           test_arch_flag_values);
  if (!err) err = test_add("test shared region size", test_shared_region_size);
  if (!err) err = test_add("test force cpusubtypes", test_force_cpusubtypes);


  return err;
}
