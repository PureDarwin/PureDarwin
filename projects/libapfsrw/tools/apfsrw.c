#include "apfsrw/apfsrw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0)
{
    fprintf(stderr,
        "usage:\n"
        "  %s info IMAGE\n"
        "  %s ls IMAGE\n"
        "  %s cat IMAGE /NAME\n",
        argv0, argv0, argv0);
}

static int print_entry(const struct apfsrw_dirent *entry, void *ctx)
{
    (void)ctx;
    printf("%llu\t%u\t%s\n", (unsigned long long)entry->file_id,
        entry->type, entry->name);
    return 0;
}

static int open_image(const char *path, struct apfsrw **fs)
{
    int err = apfsrw_open(path, 0, fs);

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

    if (strcmp(argv[1], "info") != 0 &&
        strcmp(argv[1], "ls") != 0 &&
        strcmp(argv[1], "cat") != 0) {
        usage(argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "cat") == 0 && argc != 4) {
        usage(argv[0]);
        return 2;
    }

    if ((strcmp(argv[1], "info") == 0 || strcmp(argv[1], "ls") == 0) && argc != 3) {
        usage(argv[0]);
        return 2;
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
        err = apfsrw_list_root(fs, print_entry, NULL);
    } else if (strcmp(argv[1], "cat") == 0) {
        uint8_t *data = NULL;
        size_t size = 0;

        err = apfsrw_read_root_file(fs, argv[3], &data, &size);
        if (err == APFSRW_OK) {
            if (fwrite(data, 1, size, stdout) != size)
                err = APFSRW_EIO;
            free(data);
        }
    }

    if (err != APFSRW_OK)
        fprintf(stderr, "apfsrw: %s\n", apfsrw_strerror(err));
    apfsrw_close(fs);
    return err == APFSRW_OK ? 0 : 1;
}
