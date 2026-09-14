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
        "  %s populate IMAGE SRCDIR\n",
        argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0,
        argv0, argv0, argv0, argv0);
}

static int print_entry(const struct apfsrw_dirent *entry, void *ctx)
{
    (void)ctx;
    printf("%llu\t%u\t%s\n", (unsigned long long)entry->file_id,
        entry->type, entry->name);
    return 0;
}

static int print_xattr(const struct apfsrw_xattr *x, void *ctx)
{
    (void)ctx;
    printf("%-32s flags=0x%04x size=%u\n", x->name, x->flags, x->size);
    return 0;
}

/*
 * Copy a host directory tree into the image, preserving mode and ownership.
 * Directories are created before their contents, so parents always exist.
 */
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

        /* Names needing Unicode normalisation or folding are not hashed
         * yet; skipping them keeps the volume consistent. */
        if (S_ISDIR(st.st_mode)) {
            err = apfsrw_mkdir(fs, dst, (uint16_t)(st.st_mode & 07777),
                (uint32_t)st.st_uid, (uint32_t)st.st_gid);
            if (err == APFSRW_ENOTSUP) {
                fprintf(stderr, "apfsrw: skipping %s (name)\n", dst);
                err = APFSRW_OK;
                continue;
            }
            if (err != APFSRW_OK)
                break;
            (*ndirs)++;
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
            if (err == APFSRW_ENOTSUP) {
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
            if (err == APFSRW_ENOTSUP) {
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
            /* Checkpoint periodically: superseded blocks stay unusable until
             * one lands, so a giant transaction can exhaust the container. */
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
        /* sockets, fifos and devices are skipped */
    }
    closedir(d);
    return err;
}

/* "<image>.savepoint" next to the image ("file@@offset" loses the suffix). */
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
        /* Savepoint first: a populate that dies halfway is rolled back to
         * exactly this state, here or later with `rollback`. */
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

            /* Accept a bare name as shorthand for a root-level path. */
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
    } else if (strcmp(argv[1], "ls") == 0) {
        err = apfsrw_list_dir(fs, argc >= 4 ? argv[3] : "/",
            print_entry, NULL);
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
