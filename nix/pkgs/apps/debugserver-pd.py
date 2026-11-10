#!/usr/bin/env python3
# source edits that build debugserver for PureDarwin without Foundation, libcompression or Security
# usage: debugserver-pd.py SRCDIR, or debugserver-pd.py extract USER.c OUT.c ROUTINE...
import os
import re
import sys

SECURITY_H = """#pragma once
#include <CoreFoundation/CoreFoundation.h>
// no authorization db, so developer mode reads as enabled
enum { errAuthorizationSuccess = 0, errAuthorizationDenied = -60005 };
static inline OSStatus AuthorizationRightGet(const char *r, CFDictionaryRef *d) {
  return errAuthorizationDenied;
}
"""

COMPRESSION_H = """#pragma once
#include <stddef.h>
#include <stdint.h>
// compression is never advertised, so these are never reached
typedef enum {
  COMPRESSION_LZ4 = 0x100, COMPRESSION_ZLIB = 0x205, COMPRESSION_LZMA = 0x306,
  COMPRESSION_LZ4_RAW = 0x101, COMPRESSION_LZFSE = 0x801
} compression_algorithm;
static inline size_t compression_encode_scratch_buffer_size(compression_algorithm a) { return 0; }
static inline size_t compression_encode_buffer(uint8_t *d, size_t ds, const uint8_t *s, size_t ss,
                                               void *scratch, compression_algorithm a) { return 0; }
static inline size_t compression_decode_scratch_buffer_size(compression_algorithm a) { return 0; }
static inline size_t compression_decode_buffer(uint8_t *d, size_t ds, const uint8_t *s, size_t ss,
                                               void *scratch, compression_algorithm a) { return 0; }
"""

FALLBACKS_C = """#include <errno.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

// libSystem's ptrace clears a global errno through a rip-relative ref that was
// linked against address 0, so every call takes a bus error
int ptrace(int request, pid_t pid, caddr_t addr, int data) {
  errno = 0;
  return syscall(SYS_ptrace, request, pid, addr, data);
}

// libproc implements this but libSystem does not export it yet
int proc_get_cpumon_params(pid_t pid, int *percentage, int *interval) {
  errno = ENOTSUP;
  return -1;
}
"""

OS_VERSION = """  // kern.osproductversion, as NSProcessInfo would report it
  char buf[64] = {0};
  size_t len = sizeof(buf) - 1;
  unsigned long long v[3] = {0, 0, 0};
  if (::sysctlbyname("kern.osproductversion", buf, &len, nullptr, 0) == 0)
    ::sscanf(buf, "%llu.%llu.%llu", &v[0], &v[1], &v[2]);
  if (major)
    *major = v[0];
  if (minor)
    *minor = v[1];
  if (patch)
    *patch = v[2];
  return true;"""


def edit(src, rel, fn):
    path = os.path.join(src, rel)
    with open(path) as f:
        text = f.read()
    new = fn(text)
    if new == text:
        sys.exit(f"debugserver-pd: no change in {rel}")
    with open(path, "w") as f:
        f.write(new)


def replace_function(text, signature, body):
    start = text.index(signature)
    depth = 0
    i = text.index("{", start)
    while True:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                break
        i += 1
    return text[:start] + signature + " {\n" + body + "\n}" + text[i + 1:]


IMAGE_LIST = """
#include <mach/mach_vm.h>

// dyld here has no _dyld_process_info SPI, so walk dyld_all_image_infos in the task
static void PDFindDyld(
    MachProcess *proc,
    std::vector<struct MachProcess::binary_image_information> &image_infos) {
  task_t task = proc->Task().TaskPort();
  mach_vm_address_t addr = 0;
  for (int n = 0; n < 4096; n++) {
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t obj = MACH_PORT_NULL;
    if (mach_vm_region(task, &addr, &size, VM_REGION_BASIC_INFO_64,
                       (vm_region_info_t)&info, &cnt, &obj) != KERN_SUCCESS)
      return;
    uint32_t mh[4] = {0};
    if ((info.protection & VM_PROT_READ) &&
        proc->ReadMemory(addr, sizeof(mh), mh) == sizeof(mh) &&
        mh[0] == MH_MAGIC_64 && mh[3] == MH_DYLINKER) {
      struct MachProcess::binary_image_information dyld;
      dyld.filename = "/usr/lib/dyld";
      dyld.load_address = addr;
      image_infos.push_back(dyld);
      return;
    }
    addr += size;
  }
}

static void PDReadAllImageInfos(
    MachProcess *proc,
    std::vector<struct MachProcess::binary_image_information> &image_infos) {
  DNBError err;
  nub_addr_t aii = proc->Task().GetDYLDAllImageInfosAddress(err);
  uint64_t hdr[5] = {0};
  if (aii == INVALID_NUB_ADDRESS || aii == 0 ||
      proc->ReadMemory(aii, sizeof(hdr), hdr) != sizeof(hdr)) {
    // before dyld registers its infos (a fresh launch) report dyld alone
    PDFindDyld(proc, image_infos);
    return;
  }
  uint32_t version = (uint32_t)hdr[0], count = (uint32_t)(hdr[0] >> 32);
  uint64_t array = hdr[1];

  // the dyld address is unslid until dyld has rebased itself
  uint32_t magic = 0;
  if (version >= 2 && hdr[4] &&
      proc->ReadMemory(hdr[4], sizeof(magic), &magic) == sizeof(magic) &&
      magic == MH_MAGIC_64) {
    struct MachProcess::binary_image_information dyld;
    dyld.filename = "/usr/lib/dyld";
    dyld.load_address = hdr[4];
    image_infos.push_back(dyld);
  } else {
    PDFindDyld(proc, image_infos);
  }
  for (uint32_t i = 0; array && i < count && i < 4096; i++) {
    uint64_t ent[3];
    if (proc->ReadMemory(array + i * sizeof(ent), sizeof(ent), ent) != sizeof(ent))
      break;
    char path[PATH_MAX + 1] = {0};
    if (ent[1])
      proc->ReadMemory(ent[1], PATH_MAX, path);
    struct MachProcess::binary_image_information image;
    image.filename = path;
    image.load_address = ent[0];
    image_infos.push_back(image);
  }
}

void MachProcess::GetAllLoadedBinariesViaDYLDSPI("""


def machprocess(text):
    text = text.replace("\nvoid MachProcess::GetAllLoadedBinariesViaDYLDSPI(", IMAGE_LIST, 1)
    text = text.replace(
        "    std::vector<struct binary_image_information> &image_infos) {\n  kern_return_t kern_ret;\n  if (m_dyld_process_info_create) {",
        "    std::vector<struct binary_image_information> &image_infos) {\n  kern_return_t kern_ret;\n"
        "  if (!m_dyld_process_info_create)\n    PDReadAllImageInfos(this, image_infos);\n  if (m_dyld_process_info_create) {", 1)
    text = text.replace("#import <Foundation/Foundation.h>\n\n#include \"DNBDataRef.h\"",
                        "#include \"DNBDataRef.h\"", 1)
    text = replace_function(
        text,
        "bool MachProcess::GetOSVersionNumbers(uint64_t *major, uint64_t *minor,\n"
        "                                      uint64_t *patch)",
        OS_VERSION)
    text = replace_function(
        text, "std::string MachProcess::GetMacCatalystVersionString()",
        "  return {};")
    return text


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(text)


def patch_sources(src):
    shim = os.path.join(os.path.dirname(os.path.abspath(src)), "shim")
    edit(src, "MacOSX/MachProcess.mm", machprocess)
    edit(src, "RNBRemote.cpp", lambda t: re.sub(
        r'\n\s*reply << "SupportedCompressions=[^\n]*\n', "\n", t, count=1))
    write(os.path.join(shim, "bsm", "audit_session.h"), "#include <bsm/audit.h>\n")
    write(os.path.join(shim, "Security", "Security.h"), SECURITY_H)
    write(os.path.join(shim, "compression.h"), COMPRESSION_H)
    write(os.path.join(shim, "pd_fallbacks.c"), FALLBACKS_C)


def extract_routines(user_c, out_c, names):
    # keep the mig preamble plus the named routines and their reply checks
    with open(user_c) as f:
        text = f.read()
    marker = re.compile(r"(?=#if \( __MigTypeCheck \)\n#if __MIG_check__Reply__\w+_subsystem__\n"
                        r"#if !defined\(__MIG_check__Reply__(\w+)_t__defined\))")
    starts = [m.start() for m in marker.finditer(text)]
    out = [text[:starts[0]]]
    found = set()
    for i, s in enumerate(starts):
        e = starts[i + 1] if i + 1 < len(starts) else len(text)
        name = marker.match(text, s).group(1)
        if name in names:
            out.append(text[s:e])
            found.add(name)
    if found != set(names):
        sys.exit(f"debugserver-pd: missing routines {set(names) - found}")
    with open(out_c, "w") as f:
        f.write("".join(out))


if sys.argv[1] == "extract":
    extract_routines(sys.argv[2], sys.argv[3], sys.argv[4:])
else:
    patch_sources(sys.argv[1])
