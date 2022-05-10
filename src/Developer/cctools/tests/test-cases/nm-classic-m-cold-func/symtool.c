//
//  symtool.c
//  symtool
//
//  Created by Michael Trent on 1/24/19.
//  Copyright © 2019 apple. All rights reserved.
//

#include <architecture/byte_order.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mach-o/arch.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/swap.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct lcmds {
    struct load_command** items;
    uint32_t count;
};

struct file {
    const char* path;
    unsigned char* buf;
    off_t len;
    mode_t mode;
    uint32_t nfat_arch;
    struct fat_arch* fat_archs;
    uint32_t fat_arch_idx;
    struct mach_header_64 mh;
    size_t mh_size;
    unsigned char* lcs;  // a pointer into the raw load commands
    struct lcmds* lcmds; // a c-array of pointers to swapped load commands
    bool swap;
};

static int file_read(const char* path, struct file* fb);
static int file_select_macho(struct file* fb, uint32_t index);
int file_write(const char* path, struct file* fb);

static struct lcmds* lcmds_alloc(struct file* fb);
static void lcmds_free(struct lcmds* lcmds);

static void usage(const char * __restrict format, ...);

static const char* gProgramName;
static enum NXByteOrder gByteOrder;

int main(int argc, const char * argv[])
{
    int err = 0;
    bool read_options = true;
    const char* in_path = NULL;
    const char* out_path = NULL;

    gByteOrder = NXHostByteOrder();
    gProgramName = *argv++;
    argc--;
    
    if (argc == 0)
        usage(NULL);
    
    while (argc > 0)
    {
        if (read_options && *argv && '-' == **argv)
        {
            if (0 == strcmp("-o", *argv) ||
                0 == strcmp("-output", *argv)) {
                argv++; argc--;
                if (out_path) {
                    usage("only one output file must be specified");
                }
                if (!*argv) {
                    usage("one output file must be specified");
                }
                out_path = *argv;
            }
            else if (0 == strcmp("-", *argv))
            {
                read_options = false;
            }
            else
            {
                usage("unknown option: %s", *argv);
            }
        } // if ('-' == **argv)
        else
        {
            if (in_path)
                usage("only one input file must be specified");
            in_path = *argv;
        }
        
        argv++; argc--;
    } // while (argc > 0)
    
    if (!in_path)
        usage("one input file must be specified");
    if (!out_path)
        usage("one input file must be specified");
    
    struct file file;
    struct lcmds* lcmds = NULL;

    if (0 == err) {
	err = file_read(in_path, &file);
    }

    if (0 == err) {
	err = file_select_macho(&file, 0);
    }
    
    if (0 == err) {
	lcmds = lcmds_alloc(&file);
	if (!lcmds)
	    err = -1;
    }
    
    if (0 == err) {
        for (uint32_t icmd = 0; icmd < lcmds->count; ++icmd) {
            if (lcmds->items[icmd]->cmd == LC_SYMTAB) {
                struct symtab_command *st =
                    (struct symtab_command *)lcmds->items[icmd];
                
                struct nlist_64* symbols = calloc(st->nsyms,
                                                  sizeof(*symbols));
                memcpy(symbols, (void*)(file.buf + st->symoff),
                       st->nsyms * sizeof(*symbols));
                if (file.swap)
                    swap_nlist_64(symbols, st->nsyms, gByteOrder);
                for (uint32_t isym = 0; isym < st->nsyms; ++isym) {
                    if ((symbols[isym].n_type & N_TYPE) != N_UNDF) {
                        printf("%s\n",
                               (const char*)((file.buf + st->stroff +
                                              symbols[isym].n_un.n_strx)));
                        symbols[isym].n_desc |= 0x400;
                    }
                }
                if (file.swap)
                    swap_nlist_64(symbols, st->nsyms, gByteOrder);
                memcpy((void*)(file.buf + st->symoff), symbols,
                       st->nsyms * sizeof(*symbols));
                
                free(symbols);
            }
        }
    }
    
    if (lcmds)
        lcmds_free(lcmds);
    
    if (0 == err) {
        err = file_write(out_path, &file);
    }
    
    return err ? EXIT_FAILURE : EXIT_SUCCESS;
}

/*
 * file_read() reads a Mach-O or fat file into memory and walks enough of its
 * structure to determine which architectures this file is relevant for.
 *
 * Upon success, the following struct file fields will be initialized:
 *
 *   path
 *   buf
 *   len
 *   mode
 *   nfat_arch
 *   fat_archs
 */
int file_read(const char* path, struct file* fb)
{
    memset(fb, 0, sizeof(*fb));
    
    fb->path = path;
    
    // open the file for reading
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
    
    // read the magic
    uint32_t magic;
    
    if (fb->len < sizeof(magic)) {
        fprintf(stderr, "%s error: %s file is not mach-o\n",
                gProgramName, path);
        free(fb->buf);
        return -1;
    }
    
    magic = *(uint32_t*)(fb->buf);
    
    if (magic == MH_MAGIC || magic == MH_CIGAM ||
        magic == MH_MAGIC_64 || magic == MH_CIGAM_64)
    {
        // get the mach_header size, and confirm it fits
        if (magic == MH_MAGIC || magic == MH_CIGAM) {
            fb->mh_size = sizeof(struct mach_header);
        } else {
            fb->mh_size = sizeof(struct mach_header_64);
        }
        if (fb->len < fb->mh_size) {
            fprintf(stderr, "%s error: %s file is not mach-o\n",
                    gProgramName, path);
            free(fb->buf);
            return -1;
        }
        
        // read the mach_header and swap it if needed.
        if (magic == MH_MAGIC || magic == MH_CIGAM) {
            memcpy(&fb->mh, fb->buf, fb->mh_size);
            fb->mh.reserved = 0;
        }
        else if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
            memcpy(&fb->mh, fb->buf, fb->mh_size);
        }
        if (magic == MH_CIGAM || magic == MH_CIGAM_64) {
            fb->swap = true;
            swap_mach_header_64(&fb->mh, NXHostByteOrder());
        } else {
            fb->swap = false;
        }
        
        // build a fat_arch table describing this file
        //
        // note that we don't know the alignment for this file, and while we
        // could guess at the alignment from the cputype, as other cctools do,
        // we don't acutally need this value, so we'll leave it blank.
        fb->nfat_arch = 1;
        fb->fat_archs = calloc(1, sizeof(struct fat_arch));
        fb->fat_archs[0].cputype = fb->mh.cputype;
        fb->fat_archs[0].cpusubtype = fb->mh.cpusubtype;
        fb->fat_archs[0].offset = 0;
        fb->fat_archs[0].size = (uint32_t)fb->len;
        fb->fat_archs[0].align = 0;
    }
    else if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        struct fat_header fh;
        
        // read the fat header
        if (fb->len < sizeof(fh)) {
            fprintf(stderr, "%s error: %s file is not mach-o\n",
                    gProgramName, path);
            free(fb->buf);
            return -1;
        }
        
        memcpy(&fh, fb->buf, sizeof(fh));
        swap_fat_header(&fh, NXHostByteOrder());
        
        // read the initial list of fat archs. deal with arm64ageddon binaries
        // by just reserving 1 additional spot in the fat_arch array, and
        // reading one additional item off of the arch list.
        if (fb->len < sizeof(fh) + sizeof(struct fat_arch) * (fh.nfat_arch+1)) {
            fprintf(stderr, "%s error: %s file is not mach-o\n",
                    gProgramName, path);
            free(fb->buf);
            return -1;
        }
        
        fb->nfat_arch = fh.nfat_arch;
        fb->fat_archs = malloc(sizeof(*fb->fat_archs) * (fb->nfat_arch + 1));
        memcpy(fb->fat_archs, ((struct fat_header*)fb->buf) + 1,
               sizeof(*fb->fat_archs) * (fb->nfat_arch + 1));
        swap_fat_arch(fb->fat_archs, fb->nfat_arch + 1, NXHostByteOrder());
        
        // look for arm64ageddon binaries
        bool foundARM32 = false;
        bool foundARM64 = false;
        for (uint32_t i = 0; i < fb->nfat_arch; ++i)
        {
            if (CPU_TYPE_ARM == fb->fat_archs[i].cputype)
                foundARM32 = true;
            if (CPU_TYPE_ARM64 == fb->fat_archs[i].cputype)
                foundARM64 = true;
        }
        if (foundARM32 && !foundARM64 &&
            CPU_TYPE_ARM64 == fb->fat_archs[fb->nfat_arch].cputype)
        {
            fb->nfat_arch += 1;
        }
        
        // verify the fat file contains all its subfiles.
        for (uint32_t i = 0; i < fb->nfat_arch; ++i)
        {
            if (fb->len < fb->fat_archs[i].offset + fb->fat_archs[i].size) {
                fprintf(stderr, "%s error: %s file #%d for cputype (%u, %u) "
                        "extends beyond file boundaries (%llu < %u + %u)\n",
                        gProgramName, path, i, fb->fat_archs[i].cputype,
                        fb->fat_archs[i].cpusubtype, fb->len,
                        fb->fat_archs[i].offset, fb->fat_archs[i].size);
                free(fb->buf);
                free(fb->fat_archs);
                return -1;
            }
        }
        
    }
    else {
        fprintf(stderr, "%s error: %s file is not mach-o\n",
                gProgramName, path);
        free(fb->buf);
        return -1;
    }
    
    return 0;
}

/*
 * file_select_macho() prepares the Mach-O file for reading by completing
 * initialization of the file struct and verifying the Mach-O header and load
 * command array fit in memory.
 *
 * Upon success, the following struct file fields will be initialized:
 *
 *   fat_arch_idx
 *   mh
 *   mh_size
 *   lcs
 *   lcmds
 *   swap
 */
int file_select_macho(struct file* fb, uint32_t index)
{
    if (index >= fb->nfat_arch) {
        fprintf(stderr, "%s internal error: reading beyond fat_arch array\n",
                gProgramName);
        return -1;
    }
    
    fb->fat_arch_idx = index;
    
    // re-verify the magic
    uint32_t offset = fb->fat_archs[index].offset;
    const unsigned char* buf = fb->buf + offset;
    uint32_t magic = *(uint32_t*)(buf);
    
    if (magic == MH_MAGIC || magic == MH_CIGAM ||
        magic == MH_MAGIC_64 || magic == MH_CIGAM_64)
    {
        // get the mach_header size, and confirm it fits
        if (magic == MH_MAGIC || magic == MH_CIGAM) {
            fb->mh_size = sizeof(struct mach_header);
        } else {
            fb->mh_size = sizeof(struct mach_header_64);
        }
        if (fb->len < offset + fb->mh_size) {
            fprintf(stderr, "%s error: %s file #%d for cputype (%u, %u) "
                    "is not mach-o\n",
                    gProgramName, fb->path, index, fb->fat_archs[index].cputype,
                    fb->fat_archs[index].cpusubtype);
            return -1;
        }
        
        // read the mach_header and swap it if needed.
        if (magic == MH_MAGIC || magic == MH_CIGAM) {
            memcpy(&fb->mh, buf, fb->mh_size);
            fb->mh.reserved = 0;
        }
        else if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
            memcpy(&fb->mh, buf, fb->mh_size);
        }
        if (magic == MH_CIGAM || magic == MH_CIGAM_64) {
            fb->swap = true;
            swap_mach_header_64(&fb->mh, NXHostByteOrder());
        } else {
            fb->swap = false;
        }
        
        // verify the load commands fit in the file
        if (fb->len < offset + fb->mh_size + fb->mh.sizeofcmds) {
            fprintf(stderr, "%s error: %s file #%d for cputype (%u, %u) "
                    "load command extend beyond length of file\n",
                    gProgramName, fb->path, index, fb->fat_archs[index].cputype,
                    fb->fat_archs[index].cpusubtype);
            return -1;
        }
        
        // as a convenience, compute the location of the load commands
        fb->lcs = (unsigned char*)&buf[fb->mh_size];
        
        // also cache the load commands in a convenient indexable form.
        if (fb->lcmds)
            lcmds_free(fb->lcmds);
        fb->lcmds = lcmds_alloc(fb);
        if (!fb->lcmds)
            return -1;
    }
    else {
        fprintf(stderr, "%s error: %s file #%d for cputype (%u, %u) "
                "is not mach-o\n",
                gProgramName, fb->path, index, fb->fat_archs[index].cputype,
                fb->fat_archs[index].cpusubtype);
        return -1;
    }
    
    return 0;
}

/*
 * file_write writes the entire file buffer to the specified path. the new file
 * is written into a temporary location and then moved into place. permissions
 * on the new file match that of the original file used to initialize fb.
 */
int file_write(const char* path, struct file* fb)
{
    int res = 0;
    bool warn = true;
    
    // warn if any Mach-O files source file contain code signatures.
    for (uint32_t iarch = 0; 0 == res && warn && iarch < fb->nfat_arch; ++iarch)
    {
        // prepare the mach-o for reading
        res = file_select_macho(fb, iarch);
        if (res)
            continue;
        
        // walk the load commands looking for LC_CODE_SIGNATURE
        struct lcmds* lcmds = fb->lcmds;
        for (uint32_t icmd = 0; warn && icmd < lcmds->count; ++icmd) {
            struct load_command* lc = lcmds->items[icmd];
            if (lc->cmd == LC_CODE_SIGNATURE) {
                fprintf(stderr, "%s warning: code signature will be invalid "
                        "for %s\n", gProgramName, fb->path);
                warn = false;
            }
        }
    }
    
    // compute a temporary path to hold our output file during assembly.
    size_t pathlen = strlen(path);
    const char* prefix = ".XXXXXX";
    size_t tmpsize = pathlen + strlen(prefix) + 1;
    char* tmppath = calloc(1, tmpsize);
    snprintf(tmppath, tmpsize, "%s%s", path, prefix);
    
    // open the temp file for writing
    int fd = -1;
    if (0 == res) {
        fd = mkstemp(tmppath);
        if (-1 == fd) {
            fprintf(stderr, "%s error: ", gProgramName);
            perror("mkstemp");
            res = -1;
        }
    }
    
    // write the file
    if (0 == res) {
        ssize_t wrote = write(fd, fb->buf, fb->len);
        if (wrote == -1) {
            fprintf(stderr, "%s error: %s: write: %s\n", gProgramName, tmppath,
                    strerror(errno));
            res = -1;
        }
        else if (wrote != fb->len) {
            fprintf(stderr, "%s error: %s: partial write (0x%zx of 0x%llx)\n",
                    gProgramName, tmppath, wrote, fb->len);
            res = -1;
        }
    }
    
    // close the file and move the temporary file to its final destination
    if (0 == res && close(fd)) {
        fprintf(stderr, "%s error: %s: can't close file: %s\n",
                gProgramName, tmppath, strerror(errno));
        res = -1;
    }
    
    if (0 == res && chmod(tmppath, fb->mode)) {
        fprintf(stderr, "%s error: %s: can't change file permissions: %s\n",
                gProgramName, tmppath, strerror(errno));
        res = -1;
    }
    
    if (0 == res && rename(tmppath, path)) {
        fprintf(stderr, "%s error: %s: can't rename file: %s\n", gProgramName,
                tmppath, strerror(errno));
        res = -1;
    }
    
    // try to lean up if something went wrong
    if (res) {
        unlink(tmppath);
    }
    
    free(tmppath);
    
    return res;
}

/*
 * lcmds_alloc reads the load commands from the supplied file, swaps them as
 * necessary, and stores the modified structures in a new list of load
 * commands. In this form, load commands can be looped over multiple times and
 * accessed in random order.
 *
 * BUG: for brevity, only some load command types are fully swapped. All
 * cmd and cmdsize fields will be swapped. Other fields will only be swapped
 * for segment, build version, and source version load commands.
 *
 * TODO: Fully support all load commands.
 */
struct lcmds* lcmds_alloc(struct file* fb)
{
    struct lcmds* lcmds = calloc(1, sizeof(*lcmds));
    if (!lcmds)
        return NULL;
    
    lcmds->count = fb->mh.ncmds;
    lcmds->items = calloc(lcmds->count, sizeof(*lcmds->items));
    if (!lcmds->items) {
        lcmds_free(lcmds);
        return NULL;
    }
    
    unsigned char* p = fb->lcs;
    for (uint32_t icmd = 0; icmd < fb->mh.ncmds; ++icmd)
    {
        // does the next abstract load command struct entirely fit in the
        // remaining file?
        if (fb->mh.sizeofcmds < p - fb->lcs + sizeof(struct load_command)) {
            fprintf(stderr, "%s error: load command %d extends beyond "
                    "range\n", gProgramName, icmd);
            lcmds_free(lcmds);
            return NULL;
        }
        
        // read the abstract load command
        struct load_command lc;
        memcpy(&lc, p, sizeof(lc));
        if (fb->swap) {
            swap_load_command(&lc, gByteOrder);
        }
        
        // does the next concrete load command struct entirely fit in the
        // remaining file?
        if (fb->mh.sizeofcmds < p - fb->lcs + lc.cmdsize) {
            fprintf(stderr, "%s error: load command %d (0x%X) extends "
                    "beyond range\n", gProgramName, icmd, lc.cmd);
            lcmds_free(lcmds);
            return NULL;
        }
        
        lcmds->items[icmd] = calloc(1, lc.cmdsize);
        memcpy(lcmds->items[icmd], p, lc.cmdsize);
        
        if (LC_SEGMENT == lc.cmd)
        {
            // verify the load command fits in the command buffer
            if (lc.cmdsize < sizeof(struct segment_command)) {
                fprintf(stderr,
                        "%s error: %s file for cputype (%u, %u) load "
                        "command %d (0x%x) has incorrect size (%d)\n",
                        gProgramName, fb->path, fb->mh.cputype,
                        fb->mh.cpusubtype, icmd, lc.cmd, lc.cmdsize);
                lcmds_free(lcmds);
                return NULL;
            }
            
            // swap the load command
            struct segment_command* sg =
            (struct segment_command*)lcmds->items[icmd];
            if (fb->swap) {
                swap_segment_command(sg, gByteOrder);
            }
            
            // verify the sections also fit in the command buffer
            if (lc.cmdsize != sizeof(struct segment_command) +
                sizeof(struct section) * sg->nsects) {
                fprintf(stderr,
                        "%s error: %s file for cputype (%u, %u) load "
                        "command %d (0x%x) has incorrect size (%d)\n",
                        gProgramName, fb->path, fb->mh.cputype,
                        fb->mh.cpusubtype, icmd, lc.cmd, lc.cmdsize);
                lcmds_free(lcmds);
                return NULL;
            }
            
            // swap the sections
            struct section* sc = (struct section*)(sg+1);
            if (fb->swap) {
                swap_section(sc, sg->nsects, gByteOrder);
            }
        }
        else if (LC_SEGMENT_64 == lc.cmd)
        {
            // verify the load command fits in the command buffer
            if (lc.cmdsize < sizeof(struct segment_command_64)) {
                fprintf(stderr,
                        "%s error: %s file for cputype (%u, %u) load "
                        "command %d (0x%x) has incorrect size (%d)\n",
                        gProgramName, fb->path, fb->mh.cputype,
                        fb->mh.cpusubtype, icmd, lc.cmd, lc.cmdsize);
                lcmds_free(lcmds);
                return NULL;
            }
            
            struct segment_command_64* sg =
            (struct segment_command_64*)lcmds->items[icmd];
            if (fb->swap) {
                swap_segment_command_64(sg, gByteOrder);
            }
            
            // verify the sections also fit in the command buffer
            if (lc.cmdsize != sizeof(struct segment_command_64) +
                sizeof(struct section_64) * sg->nsects) {
                fprintf(stderr,
                        "%s error: %s file for cputype (%u, %u) load "
                        "command %d (0x%x) has incorrect size (%d)\n",
                        gProgramName, fb->path, fb->mh.cputype,
                        fb->mh.cpusubtype, icmd, lc.cmd, lc.cmdsize);
                lcmds_free(lcmds);
                return NULL;
            }
            
            // swap the sections
            struct section_64* sc = (struct section_64*)(sg + 1);
            if (fb->swap) {
                swap_section_64(sc, sg->nsects, gByteOrder);
            }
        }
        else if (LC_SYMTAB == lc.cmd) {
            // verify the load command fits in the command buffer
            if (sizeof(struct symtab_command) != lc.cmdsize) {
                fprintf(stderr,
                        "%s error: %s file for cputype (%u, %u) load "
                        "command %d (0x%x) has incorrect size (%d)\n",
                        gProgramName, fb->path, fb->mh.cputype,
                        fb->mh.cpusubtype, icmd, lc.cmd, lc.cmdsize);
                lcmds_free(lcmds);
                return NULL;
            }
            
            // swap the load command
            struct symtab_command* st =
                (struct symtab_command*)lcmds->items[icmd];
            if (fb->swap) {
                swap_symtab_command(st, gByteOrder);
            }
        }
        else if (LC_VERSION_MIN_TVOS     == lc.cmd ||
                 LC_VERSION_MIN_MACOSX   == lc.cmd ||
                 LC_VERSION_MIN_WATCHOS  == lc.cmd ||
                 LC_VERSION_MIN_IPHONEOS == lc.cmd)
        {
            // verify the load command fits in the command buffer
            if (sizeof(struct version_min_command) != lc.cmdsize) {
                fprintf(stderr,
                        "%s error: %s file for cputype (%u, %u) load "
                        "command %d (0x%x) has incorrect size (%d)\n",
                        gProgramName, fb->path, fb->mh.cputype,
                        fb->mh.cpusubtype, icmd, lc.cmd, lc.cmdsize);
                lcmds_free(lcmds);
                return NULL;
            }
            
            // swap the load command
            struct version_min_command* vm =
            (struct version_min_command*)lcmds->items[icmd];
            if (fb->swap) {
                swap_version_min_command(vm, gByteOrder);
            }
        }
        else if (LC_BUILD_VERSION == lc.cmd)
        {
            // verify the load command fits in the command buffer
            if (lc.cmdsize < sizeof(struct build_version_command)) {
                fprintf(stderr,
                        "%s error: %s file for cputype (%u, %u) load "
                        "command %d (0x%x) has incorrect size (%d)\n",
                        gProgramName, fb->path, fb->mh.cputype,
                        fb->mh.cpusubtype, icmd, lc.cmd, lc.cmdsize);
                lcmds_free(lcmds);
                return NULL;
            }
            
            // swap the load command
            struct build_version_command* bv =
            (struct build_version_command*)lcmds->items[icmd];
            if (fb->swap) {
                swap_build_version_command(bv, gByteOrder);
            }
            
            // verify the build tools also fit in the command buffer
            if (lc.cmdsize < sizeof(struct build_version_command) +
                sizeof(struct build_tool_version) * bv->ntools) {
                fprintf(stderr,
                        "%s error: %s file for cputype (%u, %u) load "
                        "command %d (0x%x) has incorrect size (%d)\n",
                        gProgramName, fb->path, fb->mh.cputype,
                        fb->mh.cpusubtype, icmd, lc.cmd, lc.cmdsize);
                lcmds_free(lcmds);
                return NULL;
            }
            
            // swap the build tools
            struct build_tool_version* tv = (struct build_tool_version*)(bv+1);
            if (fb->swap) {
                swap_build_tool_version(tv, bv->ntools, gByteOrder);
            }
        }
        else if (LC_SOURCE_VERSION == lc.cmd)
        {
            // verify the load command fits in the command buffer
            if (lc.cmdsize != sizeof(struct source_version_command)) {
                fprintf(stderr,
                        "%s error: %s file for cputype (%u, %u) load "
                        "command %d (0x%x) has incorrect size (%d)\n",
                        gProgramName, fb->path, fb->mh.cputype,
                        fb->mh.cpusubtype, icmd, lc.cmd, lc.cmdsize);
                lcmds_free(lcmds);
                return NULL;
            }
            
            // swap the load command
            struct source_version_command* sv =
            (struct source_version_command*)lcmds->items[icmd];
            if (fb->swap)
                swap_source_version_command(sv, gByteOrder);
        }
        else {
            // currently lcmds->items[icmd] is unswapped load command data of
            // some size. But we were able to successfully read and swap the
            // abstract load command. Let's just write the swapped abstract
            // load command into lcmds for now, until the day comes we process
            // all defined load commands. See BUG: above.
            memcpy(lcmds->items[icmd], &lc, sizeof(lc));
        }
        
        p += lc.cmdsize;
    } // for (uint32_t icmd = 0; icmd < fb->mh.ncmds; ++icmd)
    return lcmds;
}

/*
 * lcmds_free frees memory consumed by lcmds.
 *
 * lcmds_free can be called on a partially initialied structure.
 */
void lcmds_free(struct lcmds* lcmds)
{
    for (uint32_t icmd = 0; icmd < lcmds->count; ++icmd) {
        if (lcmds->items[icmd])
            free(lcmds->items[icmd]);
    }
    if (lcmds->items)
        free(lcmds->items);
    free(lcmds);
}

/*
 * usage
 */
void usage(const char * __restrict format, ...)
{
    const char* basename = strrchr(gProgramName, '/');
    if (basename)
	basename++;
    else
	basename = gProgramName;
    
    va_list args;
    va_start(args, format);
    
    if (format) {
	fprintf(stderr, "error: ");
	vfprintf(stderr, format, args);;
	fprintf(stderr, "\n");
    }
    
    va_end(args);
    
    int size = snprintf(NULL, 0, "%s", basename);
    char* spaces = calloc(1, size + 1);
    memset(spaces, ' ', size);
    
    fprintf(stderr,
	    "usage: %s -output <output> <file>\n", basename);

    exit(EXIT_FAILURE);
}
