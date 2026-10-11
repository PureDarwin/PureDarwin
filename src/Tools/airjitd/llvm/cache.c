#ifndef __APPLE__
#define _GNU_SOURCE
#endif
#include "cache.h"

#include <dispatch/dispatch.h>
#include <errno.h>
#include <fcntl.h>
#include <llvm-c/blake3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <dlfcn.h>
#include <mach-o/loader.h>
#else
#include <link.h>
#endif

#include "../airjitd.h"

// AIRJITD_CACHE in the environment moves it, an empty value turns it off
#define PD_CACHE_DIR "/private/var/db/airjitd"
#define PD_CACHE_MAGIC 0x4353414au
#define PD_CACHE_VERSION 2
#define PD_CACHE_MAX_OBJ (64u << 20)

struct cache_header {
  uint32_t magic, version, info_bytes, obj_bytes;
  // the first bytes of the BLAKE3 of the info and the object: a torn or damaged file is not loaded
  uint8_t sum[16];
};

static const char *cache_dir(void) {
  static char dir[256];
  static int done;

  if (!done) {
    const char *e = getenv("AIRJITD_CACHE");

    snprintf(dir, sizeof(dir), "%s", e ? e : PD_CACHE_DIR);
    done = 1;
  }

  return dir[0] ? dir : 0;
}

#ifndef __APPLE__
struct code_seg {
  uintptr_t addr, start, len;
};

// the executable PT_LOAD segment that holds cs->addr
static int find_code(struct dl_phdr_info *info, size_t size, void *arg) {
  struct code_seg *cs = arg;

  (void)size;
  for (unsigned i = 0; i < info->dlpi_phnum; i++) {
    const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
    uintptr_t s = info->dlpi_addr + ph->p_vaddr;

    if (ph->p_type == PT_LOAD && (ph->p_flags & PF_X) && cs->addr >= s && cs->addr < s + ph->p_filesz) {
      cs->start = s, cs->len = ph->p_filesz;
      return 1;
    }
  }
  return 0;
}
#endif

// the LC_UUID of the image holding this code (the daemon, or Metal.framework when linked into a dylib):
// code compiled by another build never loads, as its runtime may differ
static void build_id(uint8_t id[16]) {
  static const char stamp[] = __DATE__ " " __TIME__;
#ifdef __APPLE__
  Dl_info info;
  const struct mach_header_64 *h = dladdr((const void *)&build_id, &info) && info.dli_fbase ?
                                       (const struct mach_header_64 *)info.dli_fbase : NULL;
  const uint8_t *p = h ? (const uint8_t *)(h + 1) : NULL;
  uint32_t i;

  for (i = 0; h && i < h->ncmds; i++) {
    const struct load_command *lc = (const struct load_command *)p;

    if (lc->cmd == LC_UUID) {
      memcpy(id, ((const struct uuid_command *)lc)->uuid, 16);
      return;
    }

    p += lc->cmdsize;
  }
#endif
  llvm_blake3_hasher hs;

  llvm_blake3_hasher_init(&hs);
  llvm_blake3_hasher_update(&hs, stamp, sizeof(stamp));
#ifndef __APPLE__
  // no UUID here, and nix pins __DATE__: the bytes of the code segment holding this function name the build
  {
    struct code_seg cs = { (uintptr_t)&build_id, 0, 0 };

    dl_iterate_phdr(find_code, &cs);
    if (cs.len)
      llvm_blake3_hasher_update(&hs, (const void *)cs.start, cs.len);
  }
#endif
  llvm_blake3_hasher_finalize(&hs, id, 16);
}

int pd_cache_key(const struct pd_metallib *lib, char key[PD_CACHE_KEY]) {
  static const char hex[] = "0123456789abcdef";
  static uint8_t id[16];
  static int have_id;
  uint32_t version = PD_CACHE_VERSION;
  const char *cpu, *features;
  uint8_t sum[32];
  llvm_blake3_hasher hs;
  unsigned i;

  if (!cache_dir())
    return -1;

  if (!have_id)
    build_id(id), have_id = 1;

  llvm_blake3_hasher_init(&hs);
  llvm_blake3_hasher_update(&hs, id, sizeof(id));
  llvm_blake3_hasher_update(&hs, &version, sizeof(version));
  llvm_blake3_hasher_update(&hs, lib->entry, strlen(lib->entry) + 1);
  llvm_blake3_hasher_update(&hs, lib->bitcode, lib->bitcode_len);
  pd_jit_host(&cpu, &features);
  llvm_blake3_hasher_update(&hs, cpu, strlen(cpu) + 1);
  llvm_blake3_hasher_update(&hs, features, strlen(features) + 1);
  llvm_blake3_hasher_finalize(&hs, sum, sizeof(sum));

  for (i = 0; i < sizeof(sum); i++) {
    key[2 * i] = hex[sum[i] >> 4], key[2 * i + 1] = hex[sum[i] & 15];
  }

  key[64] = 0;
  return 0;
}

static void payload_sum(const struct StageInfo *si, const uint8_t *obj, size_t len, uint8_t sum[16]) {
  llvm_blake3_hasher hs;

  llvm_blake3_hasher_init(&hs);
  llvm_blake3_hasher_update(&hs, si, sizeof(*si));
  llvm_blake3_hasher_update(&hs, obj, len);
  llvm_blake3_hasher_finalize(&hs, sum, 16);
}

static int read_all(int fd, void *buf, size_t len) {
  uint8_t *p = buf;

  while (len) {
    ssize_t n = read(fd, p, len);

    if (n <= 0)
      return -1;

    p += n, len -= (size_t)n;
  }

  return 0;
}

static int write_all(int fd, const void *buf, size_t len) {
  const uint8_t *p = buf;

  while (len) {
    ssize_t n = write(fd, p, len);

    if (n <= 0)
      return -1;

    p += n, len -= (size_t)n;
  }

  return 0;
}

int pd_cache_load(const char *key, struct StageInfo *si, uint8_t **obj, size_t *len) {
  struct cache_header h;
  struct StageInfo info;
  uint8_t sum[16], *data = 0;
  char path[384];
  int fd, rc = -1;

  snprintf(path, sizeof(path), "%s/%s.stage", cache_dir(), key);
  if ((fd = open(path, O_RDONLY)) < 0)
    return -1;

  if (read_all(fd, &h, sizeof(h)) || h.magic != PD_CACHE_MAGIC || h.version != PD_CACHE_VERSION ||
      h.info_bytes != sizeof(info) || !h.obj_bytes || h.obj_bytes > PD_CACHE_MAX_OBJ)
    goto out;

  if (read_all(fd, &info, sizeof(info)) || !(data = malloc(h.obj_bytes)) || read_all(fd, data, h.obj_bytes))
    goto out;

  payload_sum(&info, data, h.obj_bytes, sum);
  if (memcmp(sum, h.sum, sizeof(sum))) {
    logmsg("AIRJITD: cached stage %s is damaged, compiling", key);
    goto out;
  }

  *si = info;
  *obj = data, *len = h.obj_bytes;
  data = 0;
  rc = 0;
out:
  free(data);
  close(fd);
  return rc;
}

// written whole to a temporary name, then renamed: a reader sees all of it or none
static void store_now(const char *key, const struct StageInfo *si, const uint8_t *obj, size_t len) {
  static int warned;
  struct cache_header h;
  struct StageInfo info = *si;
  char path[384], tmp[400];
  int fd;

  if (!len || len > PD_CACHE_MAX_OBJ)
    return;

  info.entry = 0;
  memset(&h, 0, sizeof(h));
  h.magic = PD_CACHE_MAGIC, h.version = PD_CACHE_VERSION;
  h.info_bytes = sizeof(info), h.obj_bytes = (uint32_t)len;
  payload_sum(&info, obj, len, h.sum);

  if (mkdir(cache_dir(), 0755) && errno != EEXIST)
    goto fail;

  snprintf(path, sizeof(path), "%s/%s.stage", cache_dir(), key);
  snprintf(tmp, sizeof(tmp), "%s.%d.tmp", path, (int)getpid());

  if ((fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
    goto fail;

  if (write_all(fd, &h, sizeof(h)) || write_all(fd, &info, sizeof(info)) || write_all(fd, obj, len)) {
    close(fd);
    unlink(tmp);
    goto fail;
  }

  close(fd);
  if (rename(tmp, path)) {
    unlink(tmp);
    goto fail;
  }

  return;
fail:
  if (!warned++)
    logmsg("AIRJITD: stage cache %s not writable (errno %d), compiling every boot", cache_dir(), errno);
}

struct cache_write {
  char key[PD_CACHE_KEY];
  struct StageInfo si;
  size_t len;
  uint8_t obj[];
};

static void store_job(void *ctx) {
  struct cache_write *w = ctx;

  store_now(w->key, &w->si, w->obj, w->len);
  free(w);
}

// file writes through the guest's APFS can take seconds, so they run on their own queue and the job never waits
void pd_cache_store(const char *key, const struct StageInfo *si, const uint8_t *obj, size_t len) {
  static dispatch_queue_t q;
  struct cache_write *w;

  if (!len || len > PD_CACHE_MAX_OBJ || !(w = malloc(sizeof(*w) + len)))
    return;

  snprintf(w->key, sizeof(w->key), "%s", key);
  w->si = *si, w->len = len;
  memcpy(w->obj, obj, len);

  if (!q)
    q = dispatch_queue_create("org.puredarwin.airjitd.cache", NULL);
  dispatch_async_f(q, w, store_job);
}
