/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#include "apfsrw/apfsrw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

static void usage(const char *argv0)
{
    fprintf(stderr,
        "usage:\n"
        "  %s info IMAGE\n"
        "  %s ls IMAGE [PATH]\n"
        "  %s cat IMAGE PATH\n"
        "  %s stat IMAGE PATH\n"
        "  %s xattr IMAGE PATH\n"
        "  %s cmpstats IMAGE\n"
        "  %s rsrc IMAGE PATH\n"
        "  %s cat-id IMAGE ID\n"
        "  %s space IMAGE\n"
        "  %s write IMAGE PATH < data\n"
        "  %s mkdir IMAGE PATH\n"
        "  %s symlink IMAGE PATH TARGET\n"
        "  %s populate IMAGE SRCDIR\n"
        "  %s newvol IMAGE NAME ROLE\n"
        "  %s setxattr IMAGE PATH NAME VALUE\n"
        "  %s rmxattr IMAGE PATH NAME\n"
        "  %s rm IMAGE PATH  (files, symlinks, empty directories)\n"
        "  %s chmod IMAGE PATH OCTALMODE\n"
        "  %s fixup IMAGE  (mkapfs root/private-dir flags and modes)\n"
        "IMAGE may be FILE@@BYTEOFFSET, with a trailing @vN for volume "
        "slot N.\n",
        argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0,
        argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0);
}

static int print_entry(const struct apfsrw_dirent *entry, void *ctx)
{
    (void)ctx;
    printf("%llu\t%u\t%s\n", (unsigned long long)entry->file_id,
        entry->type, entry->name);
    return 0;
}

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

struct extract_list {
    struct apfsrw_dirent *ents;
    size_t count, cap;
};

struct extract_stats {
    unsigned long dirs, files, links, skipped, failed;
};

static int collect_entry(const struct apfsrw_dirent *entry, void *ctx)
{
    struct extract_list *l = ctx;

    if (l->count == l->cap) {
        size_t ncap = l->cap ? l->cap * 2 : 64;
        struct apfsrw_dirent *n = realloc(l->ents, ncap * sizeof(*n));

        if (n == NULL)
            return APFSRW_ENOMEM;
        l->ents = n;
        l->cap = ncap;
    }
    l->ents[l->count++] = *entry;
    return 0;
}

static int extract_skipped(const char *src, char **skip, int nskip)
{
    for (int i = 0; i < nskip; i++)
        if (strcmp(src, skip[i]) == 0)
            return 1;
    return 0;
}

// Copy SRC (a directory in the image) to host DST, decompressing files.
// Entries are collected before recursing so no btree walk is re-entered
static int extract_dir(struct apfsrw *fs, const char *src, const char *dst,
    char **skip, int nskip, struct extract_stats *st)
{
    struct extract_list l = { NULL, 0, 0 };
    int err;

    if (mkdir(dst, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "extract: mkdir %s: %s\n", dst, strerror(errno));
        return APFSRW_EIO;
    }
    err = apfsrw_list_dir(fs, src, collect_entry, &l);
    if (err != APFSRW_OK) {
        free(l.ents);
        return err;
    }
    st->dirs++;
    for (size_t i = 0; i < l.count; i++) {
        const struct apfsrw_dirent *e = &l.ents[i];
        char s[4096], d[4096];
        struct apfsrw_file_stat fst;

        snprintf(s, sizeof(s), "%s/%s", strcmp(src, "/") ? src : "", e->name);
        snprintf(d, sizeof(d), "%s/%s", dst, e->name);
        if (extract_skipped(s, skip, nskip)) {
            st->skipped++;
            continue;
        }
        if (e->type == 4) {
            err = extract_dir(fs, s, d, skip, nskip, st);
            if (err != APFSRW_OK)
                break;
            if (apfsrw_stat_file(fs, s, &fst) == APFSRW_OK)
                chmod(d, fst.mode & 07777);
        } else if (e->type == 10) {
            char target[4096];

            if (apfsrw_readlink(fs, s, target, sizeof(target), NULL) != APFSRW_OK ||
                (symlink(target, d) != 0 && errno != EEXIST)) {
                fprintf(stderr, "extract: symlink %s failed\n", s);
                st->failed++;
                continue;
            }
            st->links++;
        } else if (e->type == 8) {
            uint8_t *data = NULL;
            size_t size = 0;
            int fd, rerr = apfsrw_read_file(fs, s, &data, &size);

            if (rerr != APFSRW_OK || apfsrw_stat_file(fs, s, &fst) != APFSRW_OK) {
                fprintf(stderr, "extract: %s: %s\n", s, apfsrw_strerror(rerr));
                free(data);
                st->failed++;
                continue;
            }
            fd = open(d, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (fd < 0 || (size > 0 && write(fd, data, size) != (ssize_t)size)) {
                fprintf(stderr, "extract: write %s: %s\n", d, strerror(errno));
                if (fd >= 0)
                    close(fd);
                free(data);
                err = APFSRW_EIO;
                break;
            }
            close(fd);
            free(data);
            chmod(d, fst.mode & 07777);
            st->files++;
        } else {
            st->skipped++;
        }
    }
    free(l.ents);
    return err;
}

static int print_xattr(const struct apfsrw_xattr *x, void *ctx)
{
    (void)ctx;
    printf("%-32s flags=0x%04x size=%u\n", x->name, x->flags, x->size);
    return 0;
}

// Copy a host directory tree into the image, preserving mode and ownership.
// Directories are created before their contents, so parents always exist
static int populate_dir(struct apfsrw *fs, const char *srcdir,
    const char *dstdir, unsigned long *ndirs, unsigned long *nfiles,
    unsigned long *nlinks)
{
    DIR *d = opendir(srcdir);
    struct dirent *de;
    int err = APFSRW_OK;

    if (d == NULL) {
        fprintf(stderr, "apfsrw: opendir %s: %s\n", srcdir, strerror(errno));
        return APFSRW_EIO;
    }
    while ((de = readdir(d)) != NULL) {
        char src[4096], dst[4096];
        struct stat st;

        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        snprintf(src, sizeof(src), "%s/%s", srcdir, de->d_name);
        snprintf(dst, sizeof(dst), "%s%s%s", dstdir,
            strcmp(dstdir, "/") == 0 ? "" : "/", de->d_name);
        if (lstat(src, &st) != 0)
            continue;

        // Names needing Unicode normalisation or folding are not hashed yet. skipping them keeps
        // the volume consistent. Whatever is already on the volume is kept: populate merges
        if (S_ISDIR(st.st_mode)) {
            err = apfsrw_mkdir(fs, dst, (uint16_t)(st.st_mode & 07777),
                (uint32_t)st.st_uid, (uint32_t)st.st_gid);
            if (err == APFSRW_ENOTSUP) {
                fprintf(stderr, "apfsrw: skipping %s (name)\n", dst);
                err = APFSRW_OK;
                continue;
            }
            if (err == APFSRW_EEXIST)
                err = APFSRW_OK;
            else if (err == APFSRW_OK)
                (*ndirs)++;
            if (err != APFSRW_OK)
                break;
            err = populate_dir(fs, src, dst, ndirs, nfiles, nlinks);
            if (err != APFSRW_OK)
                break;
        } else if (S_ISLNK(st.st_mode)) {
            char target[4096];
            ssize_t n = readlink(src, target, sizeof(target) - 1);

            if (n < 0)
                continue;
            target[n] = '\0';
            err = apfsrw_symlink(fs, dst, target, (uint32_t)st.st_uid,
                (uint32_t)st.st_gid);
            if (err == APFSRW_ENOTSUP || err == APFSRW_EEXIST) {
                if (err == APFSRW_ENOTSUP)
                    fprintf(stderr, "apfsrw: skipping %s (name)\n", dst);
                err = APFSRW_OK;
                continue;
            }
            if (err != APFSRW_OK)
                break;
            (*nlinks)++;
        } else if (S_ISREG(st.st_mode)) {
            uint8_t *buf = NULL;
            size_t len = (size_t)st.st_size;
            FILE *f;

            if (len > 0) {
                buf = malloc(len);
                if (buf == NULL) {
                    err = APFSRW_ENOMEM;
                    break;
                }
                f = fopen(src, "rb");
                if (f == NULL || fread(buf, 1, len, f) != len) {
                    if (f != NULL)
                        fclose(f);
                    free(buf);
                    fprintf(stderr, "apfsrw: read %s failed\n", src);
                    err = APFSRW_EIO;
                    break;
                }
                fclose(f);
            }
            err = apfsrw_create_file(fs, dst, buf, len,
                (uint16_t)(st.st_mode & 07777), (uint32_t)st.st_uid,
                (uint32_t)st.st_gid);
            free(buf);
            if (err == APFSRW_ENOTSUP || err == APFSRW_EEXIST) {
                if (err == APFSRW_ENOTSUP)
                    fprintf(stderr, "apfsrw: skipping %s (name)\n", dst);
                err = APFSRW_OK;
                continue;
            }
            if (err != APFSRW_OK) {
                fprintf(stderr, "apfsrw: create %s: %s\n", dst,
                    apfsrw_strerror(err));
                break;
            }
            (*nfiles)++;
            // Checkpoint periodically: superseded blocks stay unusable until one lands,
            // so a giant transaction can exhaust the container
            {
                const char *fe = getenv("APFSRW_FLUSH");
                unsigned lim = fe ? (unsigned)atoi(fe) : 16384u;

            if (apfsrw_batch_pending(fs) >= lim) {
                if (apfsrw_batch_end(fs) != APFSRW_OK ||
                    apfsrw_batch_begin(fs) != APFSRW_OK) {
                    err = APFSRW_EIO;
                    break;
                }
            }
            }
        }
        // Sockets, fifos and devices are skipped
    }
    closedir(d);
    return err;
}

// "<image>.savepoint" next to the image ("file@@offset" loses the suffix)
static char *savepoint_path(const char *image)
{
    const char *at = strstr(image, "@@");
    size_t n = at ? (size_t)(at - image) : strlen(image);
    char *p = malloc(n + sizeof(".savepoint"));

    if (p == NULL)
        return NULL;
    memcpy(p, image, n);
    strcpy(p + n, ".savepoint");
    return p;
}

static uint16_t parse_role(const char *s)
{
    static const struct { const char *name; uint16_t role; } roles[] = {
        { "none", 0 }, { "system", 1 }, { "user", 2 }, { "recovery", 4 },
        { "vm", 8 }, { "preboot", 16 }, { "installer", 32 }, { "data", 64 },
        { "update", 0xc0 },
    };
    size_t i;

    for (i = 0; i < sizeof(roles) / sizeof(roles[0]); i++)
        if (strcmp(s, roles[i].name) == 0)
            return roles[i].role;
    return (uint16_t)strtoul(s, NULL, 0);
}

// Version 4, variant 1 (RFC 4122) from /dev/urandom
static void random_uuid(uint8_t uuid[16])
{
    FILE *f = fopen("/dev/urandom", "rb");
    size_t got = f ? fread(uuid, 1, 16, f) : 0;

    if (f)
        fclose(f);
    if (got != 16) {
        uint64_t t = apfsrw_now_ns();
        int i;

        for (i = 0; i < 16; i++)
            uuid[i] = (uint8_t)(t >> ((i % 8) * 8)) ^ (uint8_t)(i * 0x9d);
    }
    uuid[6] = (uint8_t)((uuid[6] & 0x0f) | 0x40);
    uuid[8] = (uint8_t)((uuid[8] & 0x3f) | 0x80);
}

static int open_image_rw(const char *path, struct apfsrw **fs)
{
    int err = apfsrw_open_xid(path, 1, 0, fs);

    if (err != APFSRW_OK)
        fprintf(stderr, "apfsrw: %s: %s\n", path, apfsrw_strerror(err));
    return err;
}

static int open_image(const char *path, struct apfsrw **fs)
{
    const char *xs = getenv("APFSRW_XID");
    int err = apfsrw_open_xid(path, 0, xs ? strtoull(xs, NULL, 0) : 0, fs);

    if (err != APFSRW_OK)
        fprintf(stderr, "apfsrw: %s: %s\n", path, apfsrw_strerror(err));
    return err;
}

int main(int argc, char **argv)
{
    struct apfsrw *fs = NULL;
    int err;

    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "populate") == 0) {
        unsigned long nd = 0, nf = 0, nl = 0;
        char *spfile;

        if (argc != 4) {
            usage(argv[0]);
            return 2;
        }
        if (open_image_rw(argv[2], &fs) != APFSRW_OK)
            return 1;
        // Savepoint first: a populate that dies halfway is rolled
        // back to exactly this state, here or later with rollback
        spfile = savepoint_path(argv[2]);
        err = spfile ? apfsrw_savepoint(fs, spfile) : APFSRW_ENOMEM;
        if (err != APFSRW_OK) {
            fprintf(stderr, "apfsrw: savepoint: %s\n", apfsrw_strerror(err));
            apfsrw_close(fs);
            return 1;
        }
        err = apfsrw_fixup_mkapfs(fs);
        if (err == APFSRW_OK)
            err = apfsrw_batch_begin(fs);
        if (err == APFSRW_OK) {
            err = populate_dir(fs, argv[3], "/", &nd, &nf, &nl);
            if (err == APFSRW_OK)
                err = apfsrw_batch_end(fs);
            else
                (void)apfsrw_batch_end(fs);
        }
        fprintf(stderr, "apfsrw: %lu dirs, %lu files, %lu symlinks\n",
            nd, nf, nl);
        if (err != APFSRW_OK) {
            int rerr;

            fprintf(stderr, "apfsrw: %s; rolling back\n",
                apfsrw_strerror(err));
            rerr = apfsrw_rollback(fs, spfile);
            fprintf(stderr, "apfsrw: rollback: %s\n",
                rerr == APFSRW_OK ? "ok" : apfsrw_strerror(rerr));
            if (rerr == APFSRW_OK)
                unlink(spfile);
        } else {
            apfsrw_savepoint_release(fs);
            unlink(spfile);
        }
        free(spfile);
        apfsrw_close(fs);
        return err == APFSRW_OK ? 0 : 1;
    }

    if (strcmp(argv[1], "role") == 0) {
        if (argc != 4) {
            usage(argv[0]);
            return 2;
        }
        if (open_image_rw(argv[2], &fs) != APFSRW_OK)
            return 1;
        err = apfsrw_set_volume_role(fs, parse_role(argv[3]));
        fprintf(stderr, "apfsrw: role: %s\n",
            err == APFSRW_OK ? "ok" : apfsrw_strerror(err));
        apfsrw_close(fs);
        return err == APFSRW_OK ? 0 : 1;
    }

    if (strcmp(argv[1], "setxattr") == 0 || strcmp(argv[1], "rmxattr") == 0) {
        int set = strcmp(argv[1], "setxattr") == 0;

        if (argc != (set ? 6 : 5)) {
            usage(argv[0]);
            return 2;
        }
        if (open_image_rw(argv[2], &fs) != APFSRW_OK)
            return 1;
        err = set ? apfsrw_set_xattr(fs, argv[3], argv[4], argv[5],
            strlen(argv[5]), 0) : apfsrw_remove_xattr(fs, argv[3], argv[4]);
        fprintf(stderr, "apfsrw: %s: %s\n", argv[1],
            err == APFSRW_OK ? "ok" : apfsrw_strerror(err));
        apfsrw_close(fs);
        return err == APFSRW_OK ? 0 : 1;
    }

    if (strcmp(argv[1], "fixup") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            return 2;
        }
        if (open_image_rw(argv[2], &fs) != APFSRW_OK)
            return 1;
        err = apfsrw_fixup_mkapfs(fs);
        fprintf(stderr, "apfsrw: fixup: %s\n",
            err == APFSRW_OK ? "ok" : apfsrw_strerror(err));
        apfsrw_close(fs);
        return err == APFSRW_OK ? 0 : 1;
    }

    if (strcmp(argv[1], "chmod") == 0) {
        struct apfsrw_attr a;

        if (argc != 5) {
            usage(argv[0]);
            return 2;
        }
        memset(&a, 0, sizeof(a));
        a.mask = APFSRW_ATTR_MODE;
        a.mode = (uint16_t)strtoul(argv[4], NULL, 8);
        if (open_image_rw(argv[2], &fs) != APFSRW_OK)
            return 1;
        err = apfsrw_setattr(fs, argv[3], &a);
        fprintf(stderr, "apfsrw: chmod: %s\n",
            err == APFSRW_OK ? "ok" : apfsrw_strerror(err));
        apfsrw_close(fs);
        return err == APFSRW_OK ? 0 : 1;
    }

    if (strcmp(argv[1], "rm") == 0) {
        if (argc != 4) {
            usage(argv[0]);
            return 2;
        }
        if (open_image_rw(argv[2], &fs) != APFSRW_OK)
            return 1;
        err = apfsrw_unlink(fs, argv[3]);
        fprintf(stderr, "apfsrw: rm: %s\n",
            err == APFSRW_OK ? "ok" : apfsrw_strerror(err));
        apfsrw_close(fs);
        return err == APFSRW_OK ? 0 : 1;
    }

    if (strcmp(argv[1], "newvol") == 0) {
        uint8_t uuid[16];
        uint32_t slot = 0;

        if (argc != 5) {
            usage(argv[0]);
            return 2;
        }
        random_uuid(uuid);
        if (open_image_rw(argv[2], &fs) != APFSRW_OK)
            return 1;
        err = apfsrw_create_volume(fs, argv[3], parse_role(argv[4]), uuid,
            &slot);
        if (err == APFSRW_OK)
            fprintf(stderr, "apfsrw: newvol: slot %u (disk?s%u)\n", slot,
                slot + 1);
        else
            fprintf(stderr, "apfsrw: newvol: %s\n", apfsrw_strerror(err));
        apfsrw_close(fs);
        return err == APFSRW_OK ? 0 : 1;
    }

    if (strcmp(argv[1], "rollback") == 0) {
        char *spfile;

        if (argc != 3 && argc != 4) {
            usage(argv[0]);
            return 2;
        }
        if (open_image_rw(argv[2], &fs) != APFSRW_OK)
            return 1;
        spfile = argc == 4 ? strdup(argv[3]) : savepoint_path(argv[2]);
        err = spfile ? apfsrw_rollback(fs, spfile) : APFSRW_ENOMEM;
        fprintf(stderr, "apfsrw: rollback: %s\n",
            err == APFSRW_OK ? "ok" : apfsrw_strerror(err));
        if (err == APFSRW_OK)
            unlink(spfile);
        free(spfile);
        apfsrw_close(fs);
        return err == APFSRW_OK ? 0 : 1;
    }

    if (strcmp(argv[1], "mkdir") == 0 || strcmp(argv[1], "symlink") == 0) {
        int is_link = strcmp(argv[1], "symlink") == 0;

        if (argc != (is_link ? 5 : 4)) {
            usage(argv[0]);
            return 2;
        }
        if (open_image_rw(argv[2], &fs) != APFSRW_OK)
            return 1;
        err = is_link ? apfsrw_symlink(fs, argv[3], argv[4], 0, 0)
                      : apfsrw_mkdir(fs, argv[3], 0755, 0, 0);
        if (err != APFSRW_OK)
            fprintf(stderr, "apfsrw: %s\n", apfsrw_strerror(err));
        apfsrw_close(fs);
        return err == APFSRW_OK ? 0 : 1;
    }

    if (strcmp(argv[1], "write") == 0) {
        uint8_t *buf = NULL;
        size_t cap = 0, len = 0;
        int c;

        if (argc != 4) {
            usage(argv[0]);
            return 2;
        }
        while ((c = fgetc(stdin)) != EOF) {
            if (len == cap) {
                size_t ncap = cap ? cap * 2 : 4096;
                uint8_t *nb = realloc(buf, ncap);

                if (nb == NULL) {
                    free(buf);
                    fprintf(stderr, "apfsrw: out of memory\n");
                    return 1;
                }
                buf = nb;
                cap = ncap;
            }
            buf[len++] = (uint8_t)c;
        }
        if (open_image_rw(argv[2], &fs) != APFSRW_OK) {
            free(buf);
            return 1;
        }
        {
            char abs[1024];

            // Accept a bare name as shorthand for a root-level path
            if (argv[3][0] == '/')
                snprintf(abs, sizeof(abs), "%s", argv[3]);
            else
                snprintf(abs, sizeof(abs), "/%s", argv[3]);
            err = apfsrw_create_file(fs, abs, buf, len, 0644, 0, 0);
        }
        free(buf);
        if (err != APFSRW_OK)
            fprintf(stderr, "apfsrw: %s\n", apfsrw_strerror(err));
        apfsrw_close(fs);
        return err == APFSRW_OK ? 0 : 1;
    }

    err = open_image(argv[2], &fs);
    if (err != APFSRW_OK)
        return 1;

    if (strcmp(argv[1], "info") == 0) {
        struct apfsrw_volume_info info;

        err = apfsrw_get_volume_info(fs, &info);
        if (err == APFSRW_OK) {
            printf("block_size: %u\n", info.block_size);
            printf("block_count: %llu\n",
                (unsigned long long)info.block_count);
            printf("xid: %llu\n", (unsigned long long)info.xid);
            printf("fs_oid: 0x%llx\n", (unsigned long long)info.fs_oid);
            printf("fs_paddr: 0x%llx\n", (unsigned long long)info.fs_paddr);
            printf("root_tree_oid: 0x%llx\n",
                (unsigned long long)info.root_tree_oid);
            printf("root_tree_paddr: 0x%llx\n",
                (unsigned long long)info.root_tree_paddr);
            printf("next_obj_id: %llu\n",
                (unsigned long long)info.next_obj_id);
            printf("num_files: %llu\n",
                (unsigned long long)info.num_files);
            printf("num_directories: %llu\n",
                (unsigned long long)info.num_directories);
        }
        if (err == APFSRW_OK) {
            struct apfsrw_volume_entry vols[100];
            uint32_t n = 0, i;

            err = apfsrw_list_volumes(fs, vols, 100, &n);
            for (i = 0; err == APFSRW_OK && i < n && i < 100; i++) {
                const uint8_t *u = vols[i].uuid;

                printf("volume slot %u%s: '%s' role 0x%x fs_oid 0x%llx "
                    "files %llu dirs %llu uuid "
                    "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-"
                    "%02X%02X%02X%02X%02X%02X\n",
                    vols[i].slot,
                    vols[i].slot == apfsrw_volume_slot(fs) ? " (open)" : "",
                    vols[i].name, vols[i].role,
                    (unsigned long long)vols[i].fs_oid,
                    (unsigned long long)vols[i].num_files,
                    (unsigned long long)vols[i].num_directories,
                    u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], u[8],
                    u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
            }
        }
    } else if (strcmp(argv[1], "ls") == 0) {
        err = apfsrw_list_dir(fs, argc >= 4 ? argv[3] : "/",
            print_entry, NULL);
    } else if (strcmp(argv[1], "extract") == 0) {
        struct extract_stats xs = { 0, 0, 0, 0, 0 };

        if (argc < 5) {
            usage(argv[0]);
            apfsrw_close(fs);
            return 2;
        }
        err = extract_dir(fs, argv[3], argv[4], argv + 5, argc - 5, &xs);
        fprintf(stderr, "apfsrw: extracted %lu dirs, %lu files, %lu symlinks; "
            "%lu skipped, %lu failed\n", xs.dirs, xs.files, xs.links,
            xs.skipped, xs.failed);
    } else if (strcmp(argv[1], "cat") == 0) {
        uint8_t *data = NULL;
        size_t size = 0;

        if (argc != 4) {
            usage(argv[0]);
            apfsrw_close(fs);
            return 2;
        }
        err = apfsrw_read_file(fs, argv[3], &data, &size);
        if (err == APFSRW_OK) {
            if (fwrite(data, 1, size, stdout) != size)
                err = APFSRW_EIO;
            free(data);
        }
    } else if (strcmp(argv[1], "stat") == 0) {
        struct apfsrw_file_stat st;

        if (argc != 4) {
            usage(argv[0]);
            apfsrw_close(fs);
            return 2;
        }
        err = apfsrw_stat_file(fs, argv[3], &st);
        if (err == APFSRW_OK) {
            printf("file_id: %llu\n", (unsigned long long)st.file_id);
            printf("stream_id: %llu\n", (unsigned long long)st.stream_id);
            printf("size: %llu\n", (unsigned long long)st.size);
            printf("extents: %llu\n", (unsigned long long)st.num_extents);
            printf("holes: %llu\n", (unsigned long long)st.num_holes);
            printf("extent_bytes: %llu\n",
                (unsigned long long)st.extent_bytes);
            printf("first_phys_block: %llu\n",
                (unsigned long long)st.first_phys_block);
            printf("first_extent_len: %llu\n",
                (unsigned long long)st.first_extent_len);
            printf("compression_type: %u\n", st.compression_type);
            printf("inline_bytes: %u\n", st.inline_bytes);
            printf("compressed_size: %llu\n",
                (unsigned long long)st.compressed_size);
        }
    } else if (strcmp(argv[1], "space") == 0) {
        struct apfsrw_space_info si;

        err = apfsrw_get_space_info(fs, &si);
        if (err == APFSRW_OK) {
            printf("spaceman_paddr: %llu\n",
                (unsigned long long)si.spaceman_paddr);
            printf("block_count: %llu\n",
                (unsigned long long)si.block_count);
            printf("chunk_count: %llu\n",
                (unsigned long long)si.chunk_count);
            printf("cib_count: %llu\n", (unsigned long long)si.cib_count);
            printf("blocks_per_chunk: %llu\n",
                (unsigned long long)si.blocks_per_chunk);
            printf("free_count: %llu\n",
                (unsigned long long)si.free_count);
            printf("chunks_seen: %llu\n",
                (unsigned long long)si.chunks_seen);
            printf("blocks_seen: %llu\n",
                (unsigned long long)si.blocks_seen);
            printf("free_seen: %llu\n", (unsigned long long)si.free_seen);
            printf("used: %llu\n",
                (unsigned long long)(si.blocks_seen - si.free_seen));
        }
    } else if (strcmp(argv[1], "cat-id") == 0) {
        uint8_t *data = NULL;
        size_t size = 0;

        if (argc != 4) {
            usage(argv[0]);
            apfsrw_close(fs);
            return 2;
        }
        err = apfsrw_read_file_by_id(fs, strtoull(argv[3], NULL, 0), &data,
            &size);
        if (err == APFSRW_OK) {
            if (fwrite(data, 1, size, stdout) != size)
                err = APFSRW_EIO;
            free(data);
        }
    } else if (strcmp(argv[1], "rsrc") == 0) {
        uint8_t *data = NULL;
        size_t size = 0;

        if (argc != 4) {
            usage(argv[0]);
            apfsrw_close(fs);
            return 2;
        }
        err = apfsrw_read_resource_fork(fs, argv[3], &data, &size);
        if (err == APFSRW_OK) {
            if (fwrite(data, 1, size, stdout) != size)
                err = APFSRW_EIO;
            free(data);
        }
    } else if (strcmp(argv[1], "xattr") == 0) {
        if (argc != 4) {
            usage(argv[0]);
            apfsrw_close(fs);
            return 2;
        }
        err = apfsrw_list_xattrs(fs, argv[3], print_xattr, NULL);
    } else if (strcmp(argv[1], "cmpstats") == 0) {
        struct apfsrw_compression_stats st;

        err = apfsrw_compression_stats(fs, &st);
        if (err == APFSRW_OK) {
            unsigned i;

            printf("compressed files: %llu\n",
                (unsigned long long)st.total);
            printf("uncompressed bytes: %llu\n",
                (unsigned long long)st.bytes);
            for (i = 0; i < APFSRW_MAX_COMPRESSION_TYPES; i++) {
                if (st.count[i] != 0) {
                    printf("  type %3u: %8llu  (sample id=%llu size=%llu "
                        "xdata=%u)\n", i,
                        (unsigned long long)st.count[i],
                        (unsigned long long)st.sample_id[i],
                        (unsigned long long)st.sample_size[i],
                        st.sample_xdata[i]);
                    if (st.inline_plus1[i] != 0) {
                        unsigned b, distinct = 0, first = 0;

                        for (b = 0; b < 256; b++)
                            if (st.head_hist[i][b] != 0) {
                                if (distinct == 0)
                                    first = b;
                                distinct++;
                            }
                        printf("            inline_len==size+1: %llu, "
                            "distinct first bytes: %u (e.g. 0x%02x)\n",
                            (unsigned long long)st.inline_plus1[i],
                            distinct, first);
                    }
                }
            }
            if (st.other != 0)
                printf("  type >=256: %llu\n",
                    (unsigned long long)st.other);
        }
    } else {
        usage(argv[0]);
        apfsrw_close(fs);
        return 2;
    }

    if (err != APFSRW_OK)
        fprintf(stderr, "apfsrw: %s\n", apfsrw_strerror(err));
    apfsrw_close(fs);
    return err == APFSRW_OK ? 0 : 1;
}
