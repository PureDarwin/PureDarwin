//
//  main.c
//  min_vers
//
//  Created by Michael Trent on 9/11/18.
//  Copyright © 2018 apple. All rights reserved.
//

#include <architecture/byte_order.h>
#include <errno.h>
#include <fcntl.h>
#include <mach-o/loader.h>
#include <mach-o/swap.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PLATFORM_DRIVERKIT
#define PLATFORM_DRIVERKIT 10
#endif /* PLATFORM_DRIVERKIT */

enum command {
    kList = 1,
    kWrite,
};

enum version_type {
    kVersMin = 1,
    kBuildVers,
    //kSourceVers,
};

struct version_type_name {
    enum version_type type;
    const char* name;
    const char* abbrev;
};

struct version_entry {
    enum version_type type;
    uint32_t platform_id;
    uint32_t minos;
    uint32_t sdk;
};

struct platform_entry {
    uint32_t platformid;
    const char* name;
    uint32_t lccmd;
};

struct options {
    const char* inPath;
    const char* outPath;

    enum command command;
    uint32_t nve;
    struct version_entry* ves;
};

struct file {
    struct mach_header_64 mh;
    size_t mh_size;
    unsigned char* buf;
    unsigned char* lcs;
    off_t len;
    mode_t mode;
    bool swap;
};

static const char* gProgramName;
static struct options gOptions;
static const struct platform_entry kPlatforms[] = {
    { PLATFORM_MACOS,               "macos",        LC_VERSION_MIN_MACOSX },
    { PLATFORM_IOS,                 "ios",          LC_VERSION_MIN_IPHONEOS },
    { PLATFORM_WATCHOS,             "watchos",      LC_VERSION_MIN_WATCHOS },
    { PLATFORM_TVOS,                "tvos",         LC_VERSION_MIN_TVOS },
    { PLATFORM_BRIDGEOS,            "bridgeos",     0 },
    { PLATFORM_MACCATALYST,         "maccatalyst",  0 },
    { PLATFORM_IOSSIMULATOR,        "iossim",       0 },
    { PLATFORM_WATCHOSSIMULATOR,    "watchossim",   0 },
    { PLATFORM_DRIVERKIT,           "driverkit",    0 },
};
static const struct version_type_name kVersionTypes[] = {
    { kVersMin,     "version-min",      "vm" },
    { kBuildVers,   "build-version",    "bv" },
};

static int read_file(const char* path, struct file* fb);
static int cmd_list(const struct file* fb);
static int cmd_write(const struct file* fb);
static int write_handle_error(int fildes, const void *buf, size_t nbyte,
                              const char* path);
static enum version_type version_type_for_str(const char* name);
static const char* version_name_for_type(enum version_type type);
static uint32_t platform_id_for_name(const char* name);
static uint32_t platform_id_for_lccmd(uint32_t lccmd);
static uint32_t platform_lccmd_for_id(uint32_t platform_id);
static const char* platform_name_for_id(uint32_t platformid);
static int parse_version(const char* rostr, uint32_t *versout);
static void print_version(uint32_t version);

static void usage(void);

int main(int argc, const char * argv[])
{
    gProgramName = *argv++;
    argc--;

    if (argc == 0) {
        usage();
    }

    gOptions.inPath = strdup(*argv);
    argv++; argc--;

    if (argc == 0) {
        fprintf(stderr, "%s error: missing command\n", gProgramName);
        usage();
    }

    if (0 == strcmp("list", *argv))
    {
        gOptions.command = kList;
        argv++; argc--;
    }
    else if (0 == strcmp("write", *argv))
    {
        // write <type> <platform> <minos> <sdk> ...
        gOptions.command = kWrite;
        argv++; argc--;

        while (argc > 1)
        {
            struct version_entry ve;

            // type
            ve.type = version_type_for_str(*argv);
            if (ve.type == 0) {
                fprintf(stderr, "%s error: unknown version type: %s\n",
                        gProgramName, *argv);
                usage();
            }
            argv++; argc--;

            // platform
            if (argc <= 1) {
                fprintf(stderr, "%s error: missing platform\n", gProgramName);
                usage();
            }

            ve.platform_id = platform_id_for_name(*argv);
            if (ve.platform_id == 0) {
                fprintf(stderr, "%s error: unknown platform %s\n", gProgramName,
                        *argv);
                usage();
            }
            argv++; argc--;

            // minos
            if (argc <= 1) {
                fprintf(stderr, "%s error: missing minos\n", gProgramName);
                usage();
            }

            if (parse_version(*argv, &ve.minos)) {
                fprintf(stderr, "%s error: bad minos %s\n", gProgramName,
                        *argv);
                usage();
            }
            argv++; argc--;

            // sdk
            if (argc <= 1) {
                fprintf(stderr, "%s error: missing sdk\n", gProgramName);
                usage();
            }

            if (parse_version(*argv, &ve.sdk)) {
                fprintf(stderr, "%s error: bad sdk %s\n", gProgramName, *argv);
                usage();
            }
            argv++; argc--;

            // add the version entry to the table
            gOptions.ves = reallocf(gOptions.ves, sizeof(*gOptions.ves) *
                                     (gOptions.nve + 1));
            gOptions.ves[gOptions.nve++] = ve;
        }

        // output file
        if (argc == 0) {
            fprintf(stderr, "%s error: missing output\n", gProgramName);
            usage();
        }

        gOptions.outPath = strdup(*argv);
        argv++; argc--;
    }
    else
    {
        fprintf(stderr, "%s error: unknown command: %s\n", gProgramName, *argv);
        usage();
    }

    int res = 0;
    struct file file;

    // read the input file
    res = read_file(gOptions.inPath, &file);
    if (!res)
    {
        // do the work!
        if (gOptions.command == kList)
            res = cmd_list(&file);
        else if (gOptions.command == kWrite)
            res = cmd_write(&file);
    }

    return res ? EXIT_FAILURE : EXIT_SUCCESS;
}

/*
 * read_file reads the file at path into the file struct pointer
 */
static int read_file(const char* path, struct file* fb)
{
    // open the file ofr reading
    int fd = open(path, O_RDONLY);
    if (-1 == fd) {
        fprintf(stderr, "%s error: %s: can't open file: %s\n",
                gProgramName, path, strerror(errno));
        return -1;
    }

    // stat the file
    struct stat sb;
    if (fstat(fd, &sb)) {
        fprintf(stderr, "%s error: %s: can't stat file: %s\n",
                gProgramName, path, strerror(errno));
        close(fd);
        return -1;
    }

    fb->len = sb.st_size;
    fb->mode = sb.st_mode;
    fb->buf = calloc(1, fb->len);

    // read the contents of the file into memory
    ssize_t readed = read(fd, fb->buf, fb->len); // that's unpossible!
    if (-1 == readed) {
        fprintf(stderr, "%s error: %s: can't read file: %s\n",
                gProgramName, path, strerror(errno));
        close(fd);
        free(fb->buf);
        return -1;
    }
    else if (readed != fb->len) {
        fprintf(stderr, "%s error: %s: partial read (0x%zx of 0x%llx)\n",
                gProgramName, path, readed, fb->len);
        close(fd);
        free(fb->buf);
        return -1;
    }

    // close the file, we're done with it.
    if (close(fd)) {
        fprintf(stderr, "%s warning: %s: can't close file: %s\n",
                gProgramName, path, strerror(errno));
    }

    // the mach header into memory
    if (fb->len < sizeof(struct mach_header_64)) {
        fprintf(stderr, "%s error: %s file too small\n", gProgramName, path);
        free(fb->buf);
        return -1;
    }

    uint32_t magic = *(uint32_t*)fb->buf;
    if (magic == MH_MAGIC || magic == MH_CIGAM) {
        fb->mh_size = sizeof(struct mach_header);
        memcpy(&fb->mh, fb->buf, fb->mh_size);
        fb->mh.reserved = 0;
    }
    else if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
        fb->mh_size = sizeof(struct mach_header_64);
        memcpy(&fb->mh, fb->buf, fb->mh_size);
    }
    else {
        fprintf(stderr, "%s error: %s file not Mach-O\n", gProgramName, path);
        free(fb->buf);
        return -1;
    }

    if (magic == MH_CIGAM || magic == MH_CIGAM_64) {
        fb->swap = true;
        swap_mach_header_64(&fb->mh, NXHostByteOrder());
    } else {
        fb->swap = false;
    }

    // verify the load commands fit in the file
    if (fb->len < fb->mh_size + fb->mh.sizeofcmds) {
        fprintf(stderr, "%s error: %s file too small\n", gProgramName, path);
        free(fb->buf);
        return -1;
    }

    // as a convenience, compute the location of the load commands
    fb->lcs = (unsigned char*)&fb->buf[fb->mh_size];

    return 0;
}

/*
 * cmd_list walks the load commands in fb and prints an entry for each version
 * load command it sees.
 */
static int cmd_list(const struct file* fb)
{
    // walk the load commands ...
    unsigned char* p = (unsigned char*)fb->lcs;
    for (uint32_t icmd = 0; icmd < fb->mh.ncmds; ++icmd)
    {
        // does the next abstract load command struct entirely fit in the
        // remaining file?
        if (fb->mh.sizeofcmds < p - fb->lcs + sizeof(struct load_command)) {
            fprintf(stderr, "%s error: load command %d extends beyond range\n",
                    gProgramName, icmd);
            return -1;
        }

        // read the abstract load command
        struct load_command lc;
        memcpy(&lc, p, sizeof(lc));
        if (fb->swap) {
            swap_load_command(&lc, NXHostByteOrder());
        }

        // does the next concrete load command struct entirely fit in the
        // remaining file?
        if (fb->mh.sizeofcmds < p - fb->lcs + lc.cmdsize) {
            fprintf(stderr,
                    "%s error: load command %d (%x) extends beyond range\n",
                    gProgramName, icmd, lc.cmd);
            return -1;
        }

        // display version_min load commands
        if (lc.cmd == LC_VERSION_MIN_MACOSX ||
            lc.cmd == LC_VERSION_MIN_IPHONEOS ||
            lc.cmd == LC_VERSION_MIN_WATCHOS ||
            lc.cmd == LC_VERSION_MIN_TVOS)
        {
            // read the concrete load command
            struct version_min_command vm;
            memcpy(&vm, p, sizeof(vm));
            if (fb->swap) {
                swap_version_min_command(&vm, NXHostByteOrder());
            }

            // version type
            printf("%s ", version_name_for_type(kVersMin));

            // version name
            const char* name =
                platform_name_for_id(platform_id_for_lccmd(lc.cmd));
            if (!name)
                name = "(unknown)";
            printf("%s ", name);

            // version minos and sdk
            print_version(vm.version);
            printf(" ");
            print_version(vm.sdk);
            printf("\n");
        }
        else if (lc.cmd == LC_BUILD_VERSION)
        {
            // read the concrete load command
            struct build_version_command bv;
            memcpy(&bv, p, sizeof(bv));
            if (fb->swap) {
                swap_build_version_command(&bv, NXHostByteOrder());
            }

            // version type
            printf("%s ", version_name_for_type(kBuildVers));

            // version name
            const char* name = platform_name_for_id(bv.platform);
            if (!name)
                name = "(unknown)";
            printf("%s ", name);

            // version minos and sdk
            print_version(bv.minos);
            printf(" ");
            print_version(bv.sdk);
            printf("\n");
        }

        // advance the current load command pointer.
       p += lc.cmdsize;
    }

    return 0;
}

/*
 * cmd_write removes existing version load commands and replaces them with
 * the 0 or more load commands specified by gOptions. care is taken to make
 * sure the new load commands fit within the remaining space in the final
 * linked image.
 */
static int cmd_write(const struct file* fb)
{
    // the write operation takes place in several phases:
    //
    //   1. loop over the load commands to find the start of section contents,
    //      and measure the size of the existing version load commands.
    //
    //   2. compute the space required to hold the new load commands, and make
    //      sure they fit in the available space.
    //
    //   3. build a new sequence of load commands and re-write the mach_header.
    //
    //   4. write the output file
    //
    // the input file is not modified in this process.

    // measure the offset to the first section and the size of the existing
    // version load commands
    off_t sectoffset = fb->len;
    uint32_t vlcsize = 0;

    unsigned char* p = (unsigned char*)fb->lcs;
    for (uint32_t icmd = 0; icmd < fb->mh.ncmds; ++icmd)
    {
        // does the next abstract load command struct entirely fit in the
        // remaining file?
        if (fb->mh.sizeofcmds < p - fb->lcs + sizeof(struct load_command)) {
            fprintf(stderr, "%s error: load command %d extends beyond range\n",
                    gProgramName, icmd);
            return -1;
        }

        // read the abstract load command
        struct load_command lc;
        memcpy(&lc, p, sizeof(lc));
        if (fb->swap) {
            swap_load_command(&lc, NXHostByteOrder());
        }

        // does the next concrete load command struct entirely fit in the
        // remaining file?
        if (fb->mh.sizeofcmds < p - fb->lcs + lc.cmdsize) {
            fprintf(stderr,
                    "%s error: load command %d (%x) extends beyond range\n",
                    gProgramName, icmd, lc.cmd);
            return -1;
        }

        if (lc.cmd == LC_SEGMENT)
        {
            // read the segment
            struct segment_command sg;
            memcpy(&sg, p, sizeof(sg));
            if (fb->swap) {
                swap_segment_command(&sg, NXHostByteOrder());
                swap_section((struct section*)(p + sizeof(sg)),
                             sg.nsects, NXHostByteOrder());
            }

            // walk the sections
            unsigned char* q = p + sizeof(sg);
            for (uint32_t isect = 0; isect < sg.nsects; ++isect)
            {
                struct section sc;
                memcpy(&sc, q, sizeof(sc));
                q += sizeof(sc);
                if (sc.offset < sectoffset)
                    sectoffset = sc.offset;
            }
        }
        if (lc.cmd == LC_SEGMENT_64)
        {
            // read the segment
            struct segment_command_64 sg;
            memcpy(&sg, p, sizeof(sg));
            if (fb->swap) {
                swap_segment_command_64(&sg, NXHostByteOrder());
                swap_section_64((struct section_64*)(p + sizeof(sg)),
                                sg.nsects, NXHostByteOrder());
            }

            // walk the sections
            unsigned char* q = p + sizeof(sg);
            for (uint32_t isect = 0; isect < sg.nsects; ++isect)
            {
                struct section_64 sc;
                memcpy(&sc, q, sizeof(sc));
                q += sizeof(sc);
                if (sc.offset > 0 && sc.offset < sectoffset)
                    sectoffset = sc.offset;
            }
        }
        if (lc.cmd == LC_VERSION_MIN_MACOSX ||
            lc.cmd == LC_VERSION_MIN_IPHONEOS ||
            lc.cmd == LC_VERSION_MIN_WATCHOS ||
            lc.cmd == LC_VERSION_MIN_TVOS ||
            lc.cmd == LC_BUILD_VERSION)
        {
            // add the concrete load command size to our running total.
            vlcsize += lc.cmdsize;
        }

        // advance the current load command pointer.
        p += lc.cmdsize;
    }

    // compute the size requirements for our new load commands by subtracting
    // away the existing commands and adding in the user-requested ones.
    uint32_t sizeofnewcmds = fb->mh.sizeofcmds;
    sizeofnewcmds -= vlcsize;

    for (uint32_t i = 0; i < gOptions.nve; ++i)
    {
        struct version_entry ve = gOptions.ves[i];

        if (ve.type == kVersMin)
            sizeofnewcmds += sizeof(struct version_min_command);
        else if (ve.type == kBuildVers)
            sizeofnewcmds += sizeof(struct build_version_command);
    }

    // verify the load commands still fit below the start of section data.
    if (sectoffset < fb->mh_size + sizeofnewcmds) {
        fprintf(stderr, "%s error: not enough space to hold load commands\n",
                gProgramName);
        return -1;
    }

    // copy all of the non-version load commands from the input file into a
    // new buffer. keep count of the number of commands written.
    unsigned char* newcmds = calloc(1, sizeofnewcmds);
    uint32_t ncmds = 0;
    unsigned char* q = newcmds;

    p = (unsigned char*)fb->lcs;
    for (uint32_t icmd = 0; icmd < fb->mh.ncmds; ++icmd)
    {
        // skip the error checking from before.

        // read the abstract load command
        struct load_command lc;
        memcpy(&lc, p, sizeof(lc));
        if (fb->swap) {
            swap_load_command(&lc, NXHostByteOrder());
        }

        // copy load commands (unswapped) from p to q, skipping version cmds.
        if (!(lc.cmd == LC_VERSION_MIN_MACOSX ||
              lc.cmd == LC_VERSION_MIN_IPHONEOS ||
              lc.cmd == LC_VERSION_MIN_WATCHOS ||
              lc.cmd == LC_VERSION_MIN_TVOS ||
              lc.cmd == LC_BUILD_VERSION)) {
            memcpy(q, p, lc.cmdsize);
            q += lc.cmdsize;
            ncmds += 1;
        }

        // advance the load command pointer
        p += lc.cmdsize;
    }

    // write the new version commands to the output buffer
    for (uint32_t i = 0; i < gOptions.nve; ++i)
    {
        struct version_entry ve = gOptions.ves[i];

        // construct a load command from the version_entry.
        if (ve.type == kVersMin)
        {
            // try to get a LC_VERSION_MIN load command for this platform.
            // if this fails, we need to die.
            //
            // TODO: Do this earlier?
            uint32_t lccmd = platform_lccmd_for_id(ve.platform_id);
            if (lccmd == 0)
            {
                // this isn't one of the four platforms supported by
                // LC_VERSION_MIN. Print a helpful message and go.
                const char* name = platform_name_for_id(ve.platform_id);
                if (name) {
                    fprintf(stderr, "%s error: version min load command not "
                            "supported for %s\n",
                            gProgramName, name);
                } else {
                    fprintf(stderr, "%s error: can't write unknown platform\n",
                            gProgramName);
                }
                free(newcmds);
                return -1;
            }

            // materialize a swapped version_min_command
            struct version_min_command vm;
            vm.cmd = lccmd;
            vm.cmdsize = sizeof(vm);
            vm.version = ve.minos;
            vm.sdk = ve.sdk;
            if (fb->swap)
                swap_version_min_command(&vm, NXHostByteOrder());

            // copy into the output buffer.
            memcpy(q, &vm, vm.cmdsize);
            q += vm.cmdsize;
        }
        else if (ve.type == kBuildVers)
        {
            struct build_version_command bv;

            // materialize a swapped build_version_command
            bv.cmd = LC_BUILD_VERSION;
            bv.cmdsize = sizeof(bv);
            bv.platform = ve.platform_id;
            bv.minos = ve.minos;
            bv.sdk = ve.sdk;
            bv.ntools = 0;
            if (fb->swap)
                swap_build_version_command(&bv, NXHostByteOrder());

            // copy into the output buffer
            memcpy(q, &bv, bv.cmdsize);
            q += bv.cmdsize;
        }

        // keep count of the number of load commands added to the output buffer
        ncmds += 1;
    }

    // compute a temporary path to hold our output file during assembly.
    size_t pathlen = strlen(gOptions.outPath);
    const char* prefix = ".XXXXXX";
    size_t tmpsize = pathlen + strlen(prefix) + 1;
    char* tmppath = calloc(1, tmpsize);
    snprintf(tmppath, tmpsize, "%s%s", gOptions.outPath, prefix);

    // open the temp file for writing
    int fd = mkstemp(tmppath);
    if (-1 == fd) {
        fprintf(stderr, "%s error: ", gProgramName);
        perror("mkstemp");
        free(newcmds);
        free(tmppath);
        return -1;
    }

    // write the mach header
    struct mach_header_64 mh;
    memcpy(&mh, &fb->mh, sizeof(mh));
    mh.ncmds = ncmds;
    mh.sizeofcmds = sizeofnewcmds;
    if (fb->swap)
        swap_mach_header_64(&mh, NXHostByteOrder());

    if (write_handle_error(fd, &mh, fb->mh_size, tmppath)) {
        close(fd);
        free(newcmds);
        free(tmppath);
        return -1;
    }

    // write the load commands
    if (write_handle_error(fd, newcmds, sizeofnewcmds, tmppath)) {
        close(fd);
        free(newcmds);
        free(tmppath);
        return -1;
    }

    free(newcmds);

    // fill zeroes between the load commands and the first section.
    size_t padlen = sectoffset - fb->mh_size - sizeofnewcmds;
    char* padbuf = calloc(1, padlen);

    if (write_handle_error(fd, padbuf, padlen, tmppath)) {
        close(fd);
        free(padbuf);
        free(tmppath);
        return -1;
    }

    free(padbuf);

    // copy the remaining data from the input file to the temp file
    if (write_handle_error(fd, &(fb->buf[sectoffset]), fb->len - sectoffset, tmppath))
    {
        close(fd);
        free(tmppath);
        return -1;
    }

    // close the file and move the temporary file to its final destination
    if (close(fd)) {
        fprintf(stderr, "%s error: %s: can't close file: %s\n",
                gProgramName, tmppath, strerror(errno));
        free(tmppath);
        return -1;
    }

    if (chmod(tmppath, fb->mode)) {
        fprintf(stderr, "%s error: %s: can't change file permissions: %s\n",
                gProgramName, tmppath, strerror(errno));
        free(tmppath);
        return -1;
    }

    if (rename(tmppath, gOptions.outPath)) {
        fprintf(stderr, "%s error: %s: can't rename file: %s\n", gProgramName,
                tmppath, strerror(errno));
        free(tmppath);
        return -1;
    }

    free(tmppath);

    return 0;
}

/*
 * write_handle_error writes nbytes from buf into the file pointed at by fd.
 * if an error occurs, a message including the path will be logged to stderr.
 * if write returns fewer bytes than requested, write_handle_error will treat
 * this as an error condition.
 */
static int write_handle_error(int fd, const void *buf, size_t nbyte,
                              const char* path)
{
    // write_handle_error writes data, prints message on error
    ssize_t wrote = write(fd, buf, nbyte);
    if (wrote == -1) {
        fprintf(stderr, "%s error: %s: write: %s\n", gProgramName, path,
                strerror(errno));
        return -1;
    } if (wrote != nbyte) {
        fprintf(stderr, "%s error: %s: partial write (0x%zx of 0x%zx)\n",
                gProgramName, path, wrote, nbyte);
        return -1;
    }

    return 0;
}

/*
 * version_type_for_str returns a version_type enum for the string in name.
 * the string may refer to a full name or an abbreviation, as recorded in the
 * VersionTypes table. returns 0 if not found.
 */
static enum version_type version_type_for_str(const char* name)
{
    int count = sizeof(kVersionTypes) / sizeof(*kVersionTypes);
    for (int i = 0; i < count; ++i)
    {
        if (0 == strcmp(name, kVersionTypes[i].name) ||
            0 == strcmp(name, kVersionTypes[i].abbrev))
            return kVersionTypes[i].type;
    }

    return 0;
}

/*
 * version_name_for_type returns the full name for a given version type.
 * returns NULL if not found.
 */
static const char* version_name_for_type(enum version_type type)
{
    int count = sizeof(kVersionTypes) / sizeof(*kVersionTypes);
    for (int i = 0; i < count; ++i)
    {
        if (type == kVersionTypes[i].type)
            return kVersionTypes[i].name;
    }

    return NULL;
}

/*
 * platform_id_for_name returns a platform enum for a given name. returns 0
 * if not found.
 */
static uint32_t platform_id_for_name(const char* name)
{
    int count = sizeof(kPlatforms) / sizeof(*kPlatforms);
    for (int i = 0; i < count; ++i)
    {
        if (0 == strcmp(name, kPlatforms[i].name))
            return kPlatforms[i].platformid;
    }
    return 0;
}

/*
 * platform_id_for_lccmd returns a platform enum for a given load command id,
 * which is expected to be an LC_VERSION_MIN_* command. returns 0 if not found.
 */
static uint32_t platform_id_for_lccmd(uint32_t lccmd)
{
    int count = sizeof(kPlatforms) / sizeof(*kPlatforms);
    for (int i = 0; i < count; ++i)
    {
        if (lccmd == kPlatforms[i].lccmd)
            return kPlatforms[i].platformid;
    }
    return 0;
}

/*
 * platform_lccmd_for_id returns a load command id for a given platform id.
 * returns 0 if the platform is not found or if that platform does not have
 * an associated version min load command.
 */
static uint32_t platform_lccmd_for_id(uint32_t platform_id)
{
    int count = sizeof(kPlatforms) / sizeof(*kPlatforms);
    for (int i = 0; i < count; ++i)
    {
        if (platform_id == kPlatforms[i].platformid)
            return kPlatforms[i].lccmd;
    }
    return 0;
}

/*
 * platform_name_for_id returns the name for a given platform id. returns NULL
 * if not found.
 */
static const char* platform_name_for_id(uint32_t platformid)
{
    int count = sizeof(kPlatforms) / sizeof(*kPlatforms);
    for (int i = 0; i < count; ++i)
    {
        if (platformid == kPlatforms[i].platformid)
            return kPlatforms[i].name;
    }
    return NULL;
}

/*
 * parse_version parses a version number out of a supplied string, and returns
 * it in the supplied version pointer. The version is packed into 4 bytes as
 * follows: XXXX.YY.ZZ. this function will fail if any of the individual
 * components are too large to fit in the available space.
 */
static int parse_version(const char* rostr, uint32_t *version)
{
    uint32_t x, y, z;
    char* str = strdup(rostr);

    // get the major version
    char* start = str;
    char* end = strchr(start, '.');
    if (end) *end = 0;
    x = (uint32_t)strtoul(start, NULL, 10);

    // get the minor version
    if (end) {
        start = end + 1;
        end = strchr(start, '.');
        if (end) *end = 0;
        y = (uint32_t)strtoul(start, NULL, 10);
    } else {
        y = 0;
    }

    // get the micro version
    if (end) {
        start = end + 1;
        end = strchr(start, '.');
        if (end) *end = 0;
        z = (uint32_t)strtoul(start, NULL, 10);
    } else {
        z = 0;
    }

    free(str);

    if (end) {
        fprintf(stderr, "%s error: version has more than 3 components: %s\n",
                gProgramName, rostr);
        return -1;
    }

    if (x > 0xFFFF) {
        fprintf(stderr, "%s error: major version is too large\n", gProgramName);
        return -1;
    }
    if (y > 0xFF) {
        fprintf(stderr, "%s error: minor version is too large\n", gProgramName);
        return -1;
    }
    if (z > 0xFF) {
        fprintf(stderr, "%s error: micro version is too large\n", gProgramName);
        return -1;
    }

    if (version) {
        *version = (x << 16) | (y << 8) | z;
    }

    return 0;
}

/*
 * print a packed version number to stdout
 */
static void print_version(uint32_t version)
{
    uint8_t y, z;
    uint16_t x;

    z = 0xff & version;
    version >>= 8;
    y = 0xff & version;
    version >>= 8;
    x = 0xffff & version;

    printf("%u.%u.%u", x, y, z);
}

/*
 * usage
 */
static void usage(void)
{
    const char* basename = strrchr(gProgramName, '/');
    if (basename)
        basename++;
    else
        basename = gProgramName;

    int size = snprintf(NULL, 0, "usage: %s", basename);
    char* spaces = calloc(1, size + 1);
    memset(spaces, ' ', size);

    fprintf(stderr, "usage: %s <input> list\n", basename);
    fprintf(stderr, "%s <input> write [(vm|bv) <platform> <minos> <sdk>] ... "
            "<output>\n", spaces);

    exit(EXIT_FAILURE);
}
