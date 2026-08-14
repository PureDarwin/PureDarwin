/*
 * Copyright (c) 2003 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <string.h>
#include <sys/syslimits.h>
#include <dirent.h>

#include "dynarray.h"
#include "NetBootServer.h"
#include "nbsp.h"

static void
NBSPEntry_print(NBSPEntryRef entry)
{
    printf("%s: path %s%s\n", entry->name, entry->path,
	   entry->is_readonly ? " [read-only]" : "");
    return;
}

int
NBSPList_count(NBSPListRef list)
{
    dynarray_t *	dlist = (dynarray_t *)list;
    
    return (dynarray_count(dlist));
}

NBSPEntryRef
NBSPList_element(NBSPListRef list, int i)
{
    dynarray_t *	dlist = (dynarray_t *)list;

    return (dynarray_element(dlist, i));
}


void
NBSPList_print(NBSPListRef list)
{
    dynarray_t *	dlist = (dynarray_t *)list;
    int 		i;

    for (i = 0; i < dynarray_count(dlist); i++) {
	NBSPEntryRef entry = (NBSPEntryRef)dynarray_element(dlist, i);
	NBSPEntry_print(entry);
    }
    return;
}

void
NBSPList_free(NBSPListRef * l)
{
    dynarray_t * list;
    if (l == NULL)
	return;
    list = *((dynarray_t * *)l);
    if (list == NULL)
	return;
    dynarray_free(list);
    free(list);
    *l = NULL;
    return;
}

static struct statfs *
get_fsstat_list(int * number)
{
    int n;
    struct statfs * stat_p;

    n = getfsstat(NULL, 0, MNT_NOWAIT);
    if (n <= 0)
	return (NULL);

    stat_p = (struct statfs *)malloc(n * sizeof(*stat_p));
    if (stat_p == NULL)
	return (NULL);

    if (getfsstat(stat_p, n * sizeof(*stat_p), MNT_NOWAIT) <= 0) {
	free(stat_p);
	return (NULL);
    }
    *number = n;
    return (stat_p);
}

static int
lookup_symlink(const char * symlink_dir,
	       const char * dir_name,
	       char * link_name, int link_name_len)
{
    DIR *		dir_p;
    int			len = 0;
    char		path[PATH_MAX];
    struct dirent *	scan;

    dir_p = opendir(symlink_dir);
    if (dir_p == NULL) {
	goto done;
    }
    while ((scan = readdir(dir_p)) != NULL) {
	char		symlink[MAXNAMLEN];
	ssize_t		symlink_len;

	if (scan->d_type != DT_LNK) {
	    continue;
	}
	snprintf(path, sizeof(path), "%s/%s",
		 symlink_dir, scan->d_name);
	symlink_len = readlink(path, symlink, sizeof(symlink) - 1);
	if (symlink_len <= 0) {
	    continue;
	}
	symlink[symlink_len] = '\0';
	if (strcmp(symlink, dir_name) == 0) {
	    strlcpy(link_name, scan->d_name, link_name_len);
	    len = strlen(link_name);
	    break;
	}
    }
 done:
    if (dir_p != NULL) {
	closedir(dir_p);
    }
    return (len);
}

NBSPListRef
NBSPList_init(const char * symlink_name, bool readonly_ok)
{
    int				i;
    dynarray_t *		list = NULL;			
    struct statfs * 		stat_p;
    int				stat_number;

    stat_p = get_fsstat_list(&stat_number);
    if (stat_p == NULL || stat_number == 0) {
	goto done;
    }

    for (i = 0; i < stat_number; i++) {
	NBSPEntryRef	entry;
	struct statfs * p = stat_p + i;
	char		sharename[MAXNAMLEN];
	int		sharename_len = 0;
	char		sharedir[PATH_MAX];
	int		sharedir_len = 0;
	char		sharelink[PATH_MAX];
	char *		root;
	struct stat	sb;

	if ((p->f_flags & MNT_LOCAL) == 0) {
	    /* skip non-local filesystems */
	    continue;
	}
	if ((p->f_flags & MNT_RDONLY) != 0 && readonly_ok == FALSE) {
	    /* skip read-only filesystems if not explicitly allowed */
	    continue;
	}
	if (strcmp(p->f_fstypename, "devfs") == 0
	    || strcmp(p->f_fstypename, "fdesc") == 0) {
	    /* don't bother with devfs, fdesc */
	    continue;
	}
	root = p->f_mntonname;
	if (strcmp(root, "/") == 0)
	    root = "";
	snprintf(sharelink, sizeof(sharelink), 
		 "%s" NETBOOT_DIRECTORY "/%s", root, symlink_name);
	if (lstat(sharelink, &sb) < 0) {
	    continue; /* doesn't exist */
	}
	if ((sb.st_mode & S_IFLNK) == 0) {
	    continue; /* not a symlink */
	}
	if (stat(sharelink, &sb) < 0) {
	    continue;
	}
	sharename_len = readlink(sharelink, sharename, sizeof(sharename) - 1);
	if (sharename_len <= 0) {
	    continue;
	}
	sharename[sharename_len] = '\0';

	/* remember the actual directory name */
	snprintf(sharedir, sizeof(sharedir), 
		 "%s" NETBOOT_DIRECTORY "/%s", root, sharename);
	sharedir_len = strlen(sharedir);
	
	if (readonly_ok) {
	    int		tftp_symlink_len;

	    /*
	     * Lookup the directory in the TFTP directory, assume that
	     * it is the sharename we want to use for both TFTP and HTTP.
	     * This isn't a safe assumption, we should independently 
	     * check/remember the TFTP/HTTP symlink names.
	     */
	    tftp_symlink_len
		= lookup_symlink(NETBOOT_TFTP_PATH "/" NETBOOT_TFTP_DIRECTORY,
				 sharedir, sharename, sizeof(sharename));
	    if (tftp_symlink_len != 0) {
		sharename_len = tftp_symlink_len;
	    }
	}
	if (list == NULL) {
	    list = (dynarray_t *)malloc(sizeof(*list));
	    if (list == NULL) {
		goto done;
	    }
	    bzero(list, sizeof(*list));
	    dynarray_init(list, free, NULL);
	}
	entry = malloc(sizeof(*entry) + sharename_len + sharedir_len + 2);
	if (entry == NULL) {
	    continue;
	}
	bzero(entry, sizeof(*entry));
	if (strcmp(p->f_fstypename, "hfs") == 0) {
	    entry->is_hfs = TRUE;
	}
	if ((p->f_flags & MNT_RDONLY) != 0) {
	    entry->is_readonly = TRUE;
	}
	entry->name = (char *)(entry + 1);
	strncpy(entry->name, sharename, sharename_len);
	entry->name[sharename_len] = '\0';
	entry->path = entry->name + sharename_len + 1;
	strncpy(entry->path, sharedir, sharedir_len);
	entry->path[sharedir_len] = '\0';
	dynarray_add((dynarray_t *)list, entry);
    }
 done:
    if (list) {
	if (dynarray_count((dynarray_t *)list) == 0) {
	    free(list);
	    list = NULL;
	}
    }
    if (stat_p != NULL) {
	free(stat_p);
    }
    return ((NBSPListRef)list);
}

#ifdef TEST_NBSP

int
main(int argc, char * argv[])
{
    bool		allow_readonly;
    NBSPListRef 	list;
    const char *	which;

    if (argc == 1) {
	which = ".sharepoint";
	allow_readonly = NBSP_READONLY_OK;
    }
    else {
	which = ".clients";
	allow_readonly = NBSP_NO_READONLY;
    }
    list = NBSPList_init(which, allow_readonly);

    if (list != NULL) {
	NBSPList_print(list);
	NBSPList_free(&list);
    }
    
    exit(0);
}

#endif /* TEST_NBSP */
