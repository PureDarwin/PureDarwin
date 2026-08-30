/*
 * Copyright (c) 2000-2010 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)nfsproto.h	8.2 (Berkeley) 3/30/95
 * FreeBSD-Id: nfsproto.h,v 1.3 1997/02/22 09:42:50 peter Exp $
 */

#ifndef _NFS_NFSPROTO_H_
#define _NFS_NFSPROTO_H_

#include <sys/appleapiopts.h>

#ifdef __APPLE_API_PRIVATE

/*
 * NFS definitions per the various NFS protocol specs:
 * Version 2 (RFC 1094), Version 3 (RFC 1813), and Version 4 (RFC 3530)
 * and various protocol-related implementation definitions.
 */

/* Only define these if nfs_prot.h hasn't been included */
#ifndef NFS_PROGRAM

#define NFS_PORT        2049
#define NFS_PROG        100003
#define NFS_VER2        2
#define NFS_VER3        3
#define NFS_VER4        4
#define NFS_V2MAXDATA   8192
#define NFS_MAXDGRAMDATA 16384
#define NFS_PREFDGRAMDATA 8192
#define NFS_MAXDATA     (1024 * 1024) /* Max data supported by kext is 2MB for x86 and 8MB for AS (8 * 64 * PAGE_SIZE) */
#define NFS_MAXPATHLEN  1024
#define NFS_MAXNAMLEN   255
#define NFS_MAXPACKET   (16 * 1024 * 1024)
#define NFS_UDPSOCKBUF  (224 * 1024)
#define NFS_FABLKSIZE   512     /* Size in bytes of a block wrt fa_blocks */

#define NFSRV_MAXDATA     NFS_MAXDATA
#define NFSRV_TCPSOCKBUF  (2 * NFSRV_MAXDATA)

#define NFSV4_MINORVERSION              0   /* V4.0 Minor version */
#define NFSV41_MINORVERSION             1   /* V4.1 Minor version */

#define NFS4_CALLBACK_PROG              0x40000000
#define NFS4_CALLBACK_PROG_VERSION      1

/* Stat numbers for NFS RPC returns */
#define NFS_OK                          0
#define NFSERR_PERM                     1
#define NFSERR_NOENT                    2
#define NFSERR_IO                       5
#define NFSERR_NXIO                     6
#define NFSERR_ACCES                    13
#define NFSERR_EXIST                    17
#define NFSERR_XDEV                     18      /* Version 3 only */
#define NFSERR_NODEV                    19
#define NFSERR_NOTDIR                   20
#define NFSERR_ISDIR                    21
#define NFSERR_INVAL                    22      /* Version 3 only */
#define NFSERR_FBIG                     27
#define NFSERR_NOSPC                    28
#define NFSERR_ROFS                     30
#define NFSERR_MLINK                    31      /* Version 3 only */
#define NFSERR_NAMETOL                  63
#define NFSERR_NOTEMPTY                 66
#define NFSERR_DQUOT                    69
#define NFSERR_STALE                    70
#define NFSERR_REMOTE                   71      /* Version 3 only */
#define NFSERR_WFLUSH                   99      /* Version 2 only */

/* NFSv3 specific errors */
#define NFSERR_BADHANDLE                10001
#define NFSERR_NOT_SYNC                 10002
#define NFSERR_BAD_COOKIE               10003
#define NFSERR_NOTSUPP                  10004
#define NFSERR_TOOSMALL                 10005
#define NFSERR_SERVERFAULT              10006
#define NFSERR_BADTYPE                  10007
#define NFSERR_JUKEBOX                  10008
#define NFSERR_TRYLATER                 NFSERR_JUKEBOX
#define NFSERR_DELAY                    NFSERR_JUKEBOX

/* NFSv4.0 specific errors */
#define NFSERR_SAME                     10009
#define NFSERR_DENIED                   10010
#define NFSERR_EXPIRED                  10011
#define NFSERR_LOCKED                   10012
#define NFSERR_GRACE                    10013
#define NFSERR_FHEXPIRED                10014
#define NFSERR_SHARE_DENIED             10015
#define NFSERR_WRONGSEC                 10016
#define NFSERR_CLID_INUSE               10017
#define NFSERR_RESOURCE                 10018
#define NFSERR_MOVED                    10019
#define NFSERR_NOFILEHANDLE             10020
#define NFSERR_MINOR_VERS_MISMATCH      10021
#define NFSERR_STALE_CLIENTID           10022
#define NFSERR_STALE_STATEID            10023
#define NFSERR_OLD_STATEID              10024
#define NFSERR_BAD_STATEID              10025
#define NFSERR_BAD_SEQID                10026
#define NFSERR_NOT_SAME                 10027
#define NFSERR_LOCK_RANGE               10028
#define NFSERR_SYMLINK                  10029
#define NFSERR_RESTOREFH                10030
#define NFSERR_LEASE_MOVED              10031
#define NFSERR_ATTRNOTSUPP              10032
#define NFSERR_NO_GRACE                 10033
#define NFSERR_RECLAIM_BAD              10034
#define NFSERR_RECLAIM_CONFLICT         10035
#define NFSERR_BADXDR                   10036
#define NFSERR_LOCKS_HELD               10037
#define NFSERR_OPENMODE                 10038
#define NFSERR_BADOWNER                 10039
#define NFSERR_BADCHAR                  10040
#define NFSERR_BADNAME                  10041
#define NFSERR_BAD_RANGE                10042
#define NFSERR_LOCK_NOTSUPP             10043
#define NFSERR_OP_ILLEGAL               10044
#define NFSERR_DEADLOCK                 10045
#define NFSERR_FILE_OPEN                10046
#define NFSERR_ADMIN_REVOKED            10047
#define NFSERR_CB_PATH_DOWN             10048

/* NFSv4.1 specific errors */
#define NFSERR_BADIOMODE                10049
#define NFSERR_BADLAYOUT                10050
#define NFSERR_BADSESSIONDIGEST         10051
#define NFSERR_BADSESSION               10052
#define NFSERR_BADSLOT                  10053
#define NFSERR_COMPLETEALREADY          10054
#define NFSERR_NOTBNDTOSESS             10055
#define NFSERR_DELEGALREADYWANT         10056
#define NFSERR_BACKCHANBUSY             10057
#define NFSERR_LAYOUTTRYLATER           10058
#define NFSERR_LAYOUTUNAVAIL            10059
#define NFSERR_NOMATCHLAYOUT            10060
#define NFSERR_RECALLCONFLICT           10061
#define NFSERR_UNKNLAYOUTTYPE           10062
#define NFSERR_SEQMISORDERED            10063
#define NFSERR_SEQUENCEPOS              10064
#define NFSERR_REQTOOBIG                10065
#define NFSERR_REPTOOBIG                10066
#define NFSERR_REPTOOBIGTOCACHE         10067
#define NFSERR_RETRYUNCACHEDREP         10068
#define NFSERR_UNSAFECOMPOUND           10069
#define NFSERR_TOOMANYOPS               10070
#define NFSERR_OPNOTINSESS              10071
#define NFSERR_HASHALGUNSUPP            10072
#define NFSERR_CLIENTIDBUSY             10074
#define NFSERR_PNFSIOHOLE               10075
#define NFSERR_SEQFALSERETRY            10076
#define NFSERR_BADHIGHSLOT              10077
#define NFSERR_DEADSESSION              10078
#define NFSERR_ENCRALGUNSUPP            10079
#define NFSERR_PNFSNOLAYOUT             10080
#define NFSERR_NOTONLYOP                10081
#define NFSERR_WRONGCRED                10082
#define NFSERR_WRONGTYPE                10083
#define NFSERR_DIRDELEGUNAVAIL          10084
#define NFSERR_REJECTDELEG              10085
#define NFSERR_RETURNCONFLICT           10086
#define NFSERR_DELEGREVOKED             10087

#define NFSERR_STALEWRITEVERF           30001   /* Fake return for nfs_commit() */
#define NFSERR_DIRBUFDROPPED            30002   /* Fake return for nfs*_readdir_rpc() */
#define NFSERR_REPLYFROMCACHE           30003   /* Fake return for nfs41_sequence_cb_get() */
#define NFSERR_SEQSTATUSERR             30004   /* Fake return for nfs41_sequence_get() */

/*
 * For gss we would like to return EAUTH when we don't have or can't get credentials,
 * but some callers don't know what to do with it, so we define our own version
 * of EAUTH to be EACCES
 */
#define NFSERR_EAUTH            EACCES

#define NFSERR_RETVOID          0x20000000 /* Return void, not error */
#define NFSERR_AUTHERR          0x40000000 /* Mark an authentication error */
#define NFSERR_RETERR           0x80000000 /* Mark an error return for V3 */

#endif /* !NFS_PROGRAM */

/* Sizes in bytes of various nfs rpc components */
#define NFSX_UNSIGNED   4

/* specific to NFS Version 2 */
#define NFSX_V2FH       32
#define NFSX_V2FATTR    68
#define NFSX_V2SATTR    32
#define NFSX_V2COOKIE   4
#define NFSX_V2STATFS   20

/* specific to NFS Version 3 */
#define NFSX_V3FHMAX            64      /* max. allowed by protocol */
#define NFSX_V3FATTR            84
#define NFSX_V3SATTR            60      /* max. all fields filled in */
#define NFSX_V3POSTOPATTR       (NFSX_V3FATTR + NFSX_UNSIGNED)
#define NFSX_V3WCCDATA          (NFSX_V3POSTOPATTR + 8 * NFSX_UNSIGNED)
#define NFSX_V3COOKIEVERF       8
#define NFSX_V3WRITEVERF        8
#define NFSX_V3CREATEVERF       8
#define NFSX_V3STATFS           52
#define NFSX_V3FSINFO           48
#define NFSX_V3PATHCONF         24

/* specific to NFS Version 4 */
#define NFS4_FHSIZE             128
#define NFS4_VERIFIER_SIZE      8
#define NFS4_OPAQUE_LIMIT       1024
#define NFS4_SESSIONID_SIZE     16

/* variants for multiple versions */
#define NFSX_FH(V)              (((V) == NFS_VER2) ? NFSX_V2FH : (NFSX_UNSIGNED + \
	                         (((V) == NFS_VER3) ? NFSX_V3FHMAX : NFS4_FHSIZE)))
#define NFSX_SRVFH(V, FH)        (((V) == NFS_VER2) ? NFSX_V2FH : (FH)->nfh_len)
#define NFSX_FATTR(V)           (((V) == NFS_VER3) ? NFSX_V3FATTR : NFSX_V2FATTR)
#define NFSX_PREOPATTR(V)       (((V) == NFS_VER3) ? (7 * NFSX_UNSIGNED) : 0)
#define NFSX_POSTOPATTR(V)      (((V) == NFS_VER3) ? (NFSX_V3FATTR + NFSX_UNSIGNED) : 0)
#define NFSX_POSTOPORFATTR(V)   (((V) == NFS_VER3) ? (NFSX_V3FATTR + NFSX_UNSIGNED) : NFSX_V2FATTR)
#define NFSX_WCCDATA(V)         (((V) == NFS_VER3) ? NFSX_V3WCCDATA : 0)
#define NFSX_WCCORFATTR(V)      (((V) == NFS_VER3) ? NFSX_V3WCCDATA : NFSX_V2FATTR)
#define NFSX_SATTR(V)           (((V) == NFS_VER3) ? NFSX_V3SATTR : NFSX_V2SATTR)
#define NFSX_COOKIEVERF(V)      (((V) == NFS_VER3) ? NFSX_V3COOKIEVERF : 0)
#define NFSX_WRITEVERF(V)       (((V) == NFS_VER3) ? NFSX_V3WRITEVERF : 0)
#define NFSX_READDIR(V)         (((V) == NFS_VER3) ? (5 * NFSX_UNSIGNED) : \
	                                (2 * NFSX_UNSIGNED))
#define NFSX_STATFS(V)          (((V) == NFS_VER3) ? NFSX_V3STATFS : NFSX_V2STATFS)

/* Only define these if nfs_prot.h hasn't been included */
#ifndef NFS_PROGRAM

/* nfs rpc procedure numbers (before version mapping) */
#define NFSPROC_NULL            0
#define NFSPROC_GETATTR         1
#define NFSPROC_SETATTR         2
#define NFSPROC_LOOKUP          3
#define NFSPROC_ACCESS          4
#define NFSPROC_READLINK        5
#define NFSPROC_READ            6
#define NFSPROC_WRITE           7
#define NFSPROC_CREATE          8
#define NFSPROC_MKDIR           9
#define NFSPROC_SYMLINK         10
#define NFSPROC_MKNOD           11
#define NFSPROC_REMOVE          12
#define NFSPROC_RMDIR           13
#define NFSPROC_RENAME          14
#define NFSPROC_LINK            15
#define NFSPROC_READDIR         16
#define NFSPROC_READDIRPLUS     17
#define NFSPROC_FSSTAT          18
#define NFSPROC_FSINFO          19
#define NFSPROC_PATHCONF        20
#define NFSPROC_COMMIT          21

#endif /* !NFS_PROGRAM */

#define NFSPROC_NOOP            22
#define NFS_NPROCS              23

/* Actual Version 2 procedure numbers */
#define NFSV2PROC_NULL          0
#define NFSV2PROC_GETATTR       1
#define NFSV2PROC_SETATTR       2
#define NFSV2PROC_NOOP          3
#define NFSV2PROC_ROOT          NFSV2PROC_NOOP  /* Obsolete */
#define NFSV2PROC_LOOKUP        4
#define NFSV2PROC_READLINK      5
#define NFSV2PROC_READ          6
#define NFSV2PROC_WRITECACHE    NFSV2PROC_NOOP  /* Obsolete */
#define NFSV2PROC_WRITE         8
#define NFSV2PROC_CREATE        9
#define NFSV2PROC_REMOVE        10
#define NFSV2PROC_RENAME        11
#define NFSV2PROC_LINK          12
#define NFSV2PROC_SYMLINK       13
#define NFSV2PROC_MKDIR         14
#define NFSV2PROC_RMDIR         15
#define NFSV2PROC_READDIR       16
#define NFSV2PROC_STATFS        17

/*
 * Constants used by the Version 3 protocol for various RPCs
 */

#define NFSV3FSINFO_LINK                0x01
#define NFSV3FSINFO_SYMLINK             0x02
#define NFSV3FSINFO_HOMOGENEOUS         0x08
#define NFSV3FSINFO_CANSETTIME          0x10

/* time setting constants */
#define NFS_TIME_DONT_CHANGE            0
#define NFS_TIME_SET_TO_SERVER          1
#define NFS_TIME_SET_TO_CLIENT          2
#define NFS4_TIME_SET_TO_SERVER         0
#define NFS4_TIME_SET_TO_CLIENT         1

/* access() constants */
#define NFS_ACCESS_READ                 0x01
#define NFS_ACCESS_LOOKUP               0x02
#define NFS_ACCESS_MODIFY               0x04
#define NFS_ACCESS_EXTEND               0x08
#define NFS_ACCESS_DELETE               0x10
#define NFS_ACCESS_EXECUTE              0x20
#define NFS_ACCESS_ALL (NFS_ACCESS_READ | NFS_ACCESS_MODIFY             \
	                 | NFS_ACCESS_EXTEND | NFS_ACCESS_EXECUTE       \
	                 | NFS_ACCESS_DELETE | NFS_ACCESS_LOOKUP)

/* NFS WRITE how constants */
#define NFS_WRITE_UNSTABLE              0
#define NFS_WRITE_DATASYNC              1
#define NFS_WRITE_FILESYNC              2

/* NFS CREATE types */
#define NFS_CREATE_UNCHECKED            0
#define NFS_CREATE_GUARDED              1
#define NFS_CREATE_EXCLUSIVE            2 /* Deprecated in NFSv4.1. */
#define NFS_CREATE_EXCLUSIVE4_1         3 /* New to NFSv4.1 */

/* Only define these if nfs_prot.h hasn't been included */
#ifndef NFS_PROGRAM
/* NFS object types */
typedef enum { NFNON=0, NFREG=1, NFDIR=2, NFBLK=3, NFCHR=4, NFLNK=5,
	       NFSOCK=6, NFFIFO=7, NFATTRDIR=8, NFNAMEDATTR=9 } nfstype;
#endif /* !NFS_PROGRAM */

/*
 * File Handle (32 bytes for version 2), variable up to 64 for version 3
 * and variable up to 128 bytes for version 4.
 * File Handles of up to NFS_SMALLFH in size are stored directly in the
 * nfs node, whereas larger ones are malloc'd. (This never happens when
 * NFS_SMALLFH is set to the largest size.)
 * NFS_SMALLFH should be in the range of 32 to 64 and be divisible by 4.
 */
#ifndef NFS_SMALLFH
#define NFS_SMALLFH     64
#endif

/*
 * NFS attribute management stuff
 */
#define NFS_ATTR_BITMAP_LEN     3
#define NFS_BITMAP_SET(B, I)    (((uint32_t *)(B))[(I)/32] |= 1U<<((I)%32))
#define NFS_BITMAP_CLR(B, I)    (((uint32_t *)(B))[(I)/32] &= ~(1U<<((I)%32)))
#define NFS_BITMAP_ISSET(B, I)  (((uint32_t *)(B))[(I)/32] & (1U<<((I)%32)))
#define NFS_BITMAP_COPY_ATTR(FROM, TO, WHICH, ATTR) \
	do { \
	        if (NFS_BITMAP_ISSET(((FROM)->nva_bitmap), (NFS_FATTR_##WHICH))) { \
	                (TO)->nva_##ATTR = (FROM)->nva_##ATTR; \
	                NFS_BITMAP_SET(((TO)->nva_bitmap), (NFS_FATTR_##WHICH)); \
	        } \
	} while (0)
#define NFS_BITMAP_COPY_TIME(FROM, TO, WHICH, ATTR) \
	do { \
	        if (NFS_BITMAP_ISSET(((FROM)->nva_bitmap), (NFS_FATTR_TIME_##WHICH))) { \
	                (TO)->nva_timesec[NFSTIME_##ATTR] = (FROM)->nva_timesec[NFSTIME_##ATTR]; \
	                (TO)->nva_timensec[NFSTIME_##ATTR] = (FROM)->nva_timensec[NFSTIME_##ATTR]; \
	                NFS_BITMAP_SET(((TO)->nva_bitmap), (NFS_FATTR_TIME_##WHICH)); \
	        } \
	} while (0)
#define NFS_BITMAP_ZERO(B, L) \
	do { \
	        int __i; \
	        for (__i=0; __i < (L); __i++) \
	                ((uint32_t*)(B))[__i] = 0; \
	} while (0)

#define NFS_CLEAR_ATTRIBUTES(A) NFS_BITMAP_ZERO((A), NFS_ATTR_BITMAP_LEN)
#define NFS_COPY_ATTRIBUTES(SRC, DST) \
	do { \
	int __i; \
	for (__i=0; __i < NFS_ATTR_BITMAP_LEN; __i++) \
	        ((uint32_t*)(DST))[__i] = ((uint32_t*)(SRC))[__i]; \
	} while (0)

/* NFS attributes */
#define NFS_FATTR_SUPPORTED_ATTRS               0
#define NFS_FATTR_TYPE                          1
#define NFS_FATTR_FH_EXPIRE_TYPE                2
#define NFS_FATTR_CHANGE                        3
#define NFS_FATTR_SIZE                          4
#define NFS_FATTR_LINK_SUPPORT                  5
#define NFS_FATTR_SYMLINK_SUPPORT               6
#define NFS_FATTR_NAMED_ATTR                    7
#define NFS_FATTR_FSID                          8
#define NFS_FATTR_UNIQUE_HANDLES                9
#define NFS_FATTR_LEASE_TIME                    10
#define NFS_FATTR_RDATTR_ERROR                  11
#define NFS_FATTR_FILEHANDLE                    19
#define NFS_FATTR_ACL                           12
#define NFS_FATTR_ACLSUPPORT                    13
#define NFS_FATTR_ARCHIVE                       14
#define NFS_FATTR_CANSETTIME                    15
#define NFS_FATTR_CASE_INSENSITIVE              16
#define NFS_FATTR_CASE_PRESERVING               17
#define NFS_FATTR_CHOWN_RESTRICTED              18
#define NFS_FATTR_FILEID                        20
#define NFS_FATTR_FILES_AVAIL                   21
#define NFS_FATTR_FILES_FREE                    22
#define NFS_FATTR_FILES_TOTAL                   23
#define NFS_FATTR_FS_LOCATIONS                  24
#define NFS_FATTR_HIDDEN                        25
#define NFS_FATTR_HOMOGENEOUS                   26
#define NFS_FATTR_MAXFILESIZE                   27
#define NFS_FATTR_MAXLINK                       28
#define NFS_FATTR_MAXNAME                       29
#define NFS_FATTR_MAXREAD                       30
#define NFS_FATTR_MAXWRITE                      31
#define NFS_FATTR_MIMETYPE                      32
#define NFS_FATTR_MODE                          33
#define NFS_FATTR_NO_TRUNC                      34
#define NFS_FATTR_NUMLINKS                      35
#define NFS_FATTR_OWNER                         36
#define NFS_FATTR_OWNER_GROUP                   37
#define NFS_FATTR_QUOTA_AVAIL_HARD              38
#define NFS_FATTR_QUOTA_AVAIL_SOFT              39
#define NFS_FATTR_QUOTA_USED                    40
#define NFS_FATTR_RAWDEV                        41
#define NFS_FATTR_SPACE_AVAIL                   42
#define NFS_FATTR_SPACE_FREE                    43
#define NFS_FATTR_SPACE_TOTAL                   44
#define NFS_FATTR_SPACE_USED                    45
#define NFS_FATTR_SYSTEM                        46
#define NFS_FATTR_TIME_ACCESS                   47
#define NFS_FATTR_TIME_ACCESS_SET               48
#define NFS_FATTR_TIME_BACKUP                   49
#define NFS_FATTR_TIME_CREATE                   50
#define NFS_FATTR_TIME_DELTA                    51
#define NFS_FATTR_TIME_METADATA                 52
#define NFS_FATTR_TIME_MODIFY                   53
#define NFS_FATTR_TIME_MODIFY_SET               54
#define NFS_FATTR_MOUNTED_ON_FILEID             55
#define NFS_FATTR_DIR_NOTIF_DELAY               56
#define NFS_FATTR_DIRENT_NOTIF_DELAY            57
#define NFS_FATTR_DACL                          58
#define NFS_FATTR_SACL                          59
#define NFS_FATTR_CHANGE_POLICY                 60
#define NFS_FATTR_FS_STATUS                     61
#define NFS_FATTR_FS_LAYOUT_TYPE                62
#define NFS_FATTR_LAYOUT_HINT                   63
#define NFS_FATTR_LAYOUT_TYPE                   64
#define NFS_FATTR_LAYOUT_BLKSIZE                65
#define NFS_FATTR_LAYOUT_ALIGNMENT              66
#define NFS_FATTR_FS_LOCATIONS_INFO             67
#define NFS_FATTR_MDSTHRESHOLD                  68
#define NFS_FATTR_RETENTION_GET                 69
#define NFS_FATTR_RETENTION_SET                 70
#define NFS_FATTR_RETENTEVT_GET                 71
#define NFS_FATTR_RETENTEVT_SET                 72
#define NFS_FATTR_RETENTION_HOLD                73
#define NFS_FATTR_MODE_SET_MASKED               74
#define NFS_FATTR_SUPPATTR_EXCLCREAT            75
#define NFS_FATTR_FS_CHARSET_CAP                76

/*
 * NFS OPEN constants
 */
/* open type */
#define NFS_OPEN_NOCREATE                       0
#define NFS_OPEN_CREATE                         1

/* delegation space limit */
#define NFS_LIMIT_SIZE                          1
#define NFS_LIMIT_BLOCKS                        2

/* access/deny modes */
#define NFS_OPEN_SHARE_ACCESS_NONE              0x00000000
#define NFS_OPEN_SHARE_ACCESS_READ              0x00000001
#define NFS_OPEN_SHARE_ACCESS_WRITE             0x00000002
#define NFS_OPEN_SHARE_ACCESS_BOTH              0x00000003
#define NFS_OPEN_SHARE_DENY_NONE                0x00000000
#define NFS_OPEN_SHARE_DENY_READ                0x00000001
#define NFS_OPEN_SHARE_DENY_WRITE               0x00000002
#define NFS_OPEN_SHARE_DENY_BOTH                0x00000003

/* flags for share_access field of OPEN4args */
#define NFS_OPEN4_SHARE_ACCESS_WANT_DELEG_MASK                    0x0FF00
#define NFS_OPEN4_SHARE_ACCESS_WANT_NO_PREFERENCE                 0x00000
#define NFS_OPEN4_SHARE_ACCESS_WANT_READ_DELEG                    0x00100
#define NFS_OPEN4_SHARE_ACCESS_WANT_WRITE_DELEG                   0x00200
#define NFS_OPEN4_SHARE_ACCESS_WANT_ANY_DELEG                     0x00300
#define NFS_OPEN4_SHARE_ACCESS_WANT_NO_DELEG                      0x00400
#define NFS_OPEN4_SHARE_ACCESS_WANT_CANCEL                        0x00500
#define NFS_OPEN4_SHARE_ACCESS_WANT_SIGNAL_DELEG_WHEN_RESRC_AVAIL 0x10000
#define NFS_OPEN4_SHARE_ACCESS_WANT_PUSH_DELEG_WHEN_UNCONTENDED   0x20000

/* delegation types */
#define NFS_OPEN_DELEGATE_NONE                  0
#define NFS_OPEN_DELEGATE_READ                  1
#define NFS_OPEN_DELEGATE_WRITE                 2
#define NFS_OPEN_DELEGATE_NONE_EXT              3 /* New to v4.1 */

/* delegation claim types */
#define NFS_CLAIM_NULL                          0
#define NFS_CLAIM_PREVIOUS                      1
#define NFS_CLAIM_DELEGATE_CUR                  2
#define NFS_CLAIM_DELEGATE_PREV                 3
#define NFS_CLAIM_FH                            4 /* New to v4.1 */
#define NFS_CLAIM_DELEG_CUR_FH                  5 /* New to v4.1 */
#define NFS_CLAIM_DELEG_PREV_FH                 6 /* New to v4.1 */

/* why_no_delegation4 */
#define NFS_WND4_NOT_WANTED                     0
#define NFS_WND4_CONTENTION                     1
#define NFS_WND4_RESOURCE                       2
#define NFS_WND4_NOT_SUPP_FTYPE                 3
#define NFS_WND4_WRITE_DELEG_NOT_SUPP_FTYPE     4
#define NFS_WND4_NOT_SUPP_UPGRADE               5
#define NFS_WND4_NOT_SUPP_DOWNGRADE             6
#define NFS_WND4_CANCELLED                      7
#define NFS_WND4_IS_DIR                         8

/* open result flags */
#define NFS_OPEN_RESULT_CONFIRM                 0x00000002
#define NFS_OPEN_RESULT_LOCKTYPE_POSIX          0x00000004
#define NFS_OPEN_RESULT_PRESERVE_UNLINKED       0x00000008
#define NFS_OPEN_RESULT_MAY_NOTIFY_LOCK         0x00000020

/* NFS lock types */
#define NFS_LOCK_TYPE_READ                      1
#define NFS_LOCK_TYPE_WRITE                     2
#define NFS_LOCK_TYPE_READW                     3 /* "blocking" */
#define NFS_LOCK_TYPE_WRITEW                    4 /* "blocking" */

/* NFSv4 RPC procedures */
#define NFSPROC4_NULL                           0
#define NFSPROC4_COMPOUND                       1
#define NFSPROC4_CB_NULL                        0
#define NFSPROC4_CB_COMPOUND                    1

/* NFSv4 opcodes */
#define NFS_OP_ACCESS                           3
#define NFS_OP_CLOSE                            4
#define NFS_OP_COMMIT                           5
#define NFS_OP_CREATE                           6
#define NFS_OP_DELEGPURGE                       7
#define NFS_OP_DELEGRETURN                      8
#define NFS_OP_GETATTR                          9
#define NFS_OP_GETFH                            10
#define NFS_OP_LINK                             11
#define NFS_OP_LOCK                             12
#define NFS_OP_LOCKT                            13
#define NFS_OP_LOCKU                            14
#define NFS_OP_LOOKUP                           15
#define NFS_OP_LOOKUPP                          16
#define NFS_OP_NVERIFY                          17
#define NFS_OP_OPEN                             18
#define NFS_OP_OPENATTR                         19
#define NFS_OP_OPEN_CONFIRM                     20
#define NFS_OP_OPEN_DOWNGRADE                   21
#define NFS_OP_PUTFH                            22
#define NFS_OP_PUTPUBFH                         23
#define NFS_OP_PUTROOTFH                        24
#define NFS_OP_READ                             25
#define NFS_OP_READDIR                          26
#define NFS_OP_READLINK                         27
#define NFS_OP_REMOVE                           28
#define NFS_OP_RENAME                           29
#define NFS_OP_RENEW                            30
#define NFS_OP_RESTOREFH                        31
#define NFS_OP_SAVEFH                           32
#define NFS_OP_SECINFO                          33
#define NFS_OP_SETATTR                          34
#define NFS_OP_SETCLIENTID                      35
#define NFS_OP_SETCLIENTID_CONFIRM              36
#define NFS_OP_VERIFY                           37
#define NFS_OP_WRITE                            38
#define NFS_OP_RELEASE_LOCKOWNER                39

/* NFSv4 opcodes */
#define NFS_V4_OP_COUNT                         40

/* NFSv4.1 opcodes */
#define NFS_OP_BACKCHANNELCTL                   40
#define NFS_OP_BINDCONNTOSESS                   41
#define NFS_OP_EXCHANGEID                       42
#define NFS_OP_CREATESESSION                    43
#define NFS_OP_DESTROYSESSION                   44
#define NFS_OP_FREESTATEID                      45
#define NFS_OP_GETDIRDELEG                      46
#define NFS_OP_GETDEVINFO                       47
#define NFS_OP_GETDEVLIST                       48
#define NFS_OP_LAYOUTCOMMIT                     49
#define NFS_OP_LAYOUTGET                        50
#define NFS_OP_LAYOUTRETURN                     51
#define NFS_OP_SECINFONONAME                    52
#define NFS_OP_SEQUENCE                         53
#define NFS_OP_SETSSV                           54
#define NFS_OP_TESTSTATEID                      55
#define NFS_OP_WANTDELEG                        56
#define NFS_OP_DESTROYCLIENTID                  57
#define NFS_OP_RECLAIMCOMPL                     58

/* NFSv4.1 opcodes */
#define NFS_V41_OP_COUNT                        59

/* illegal op code */
#define NFS_OP_ILLEGAL                          10044

/* NFSv4 callback opcodes */
#define NFS_OP_CB_GETATTR                       3
#define NFS_OP_CB_RECALL                        4

/* NFSv4 callback opcodes */
#define NFS_V4_OP_CB_COUNT                      5

/* NFSv4.1 callback opcodes */
#define NFS_OP_CB_LAYOUTRECALL                  5
#define NFS_OP_CB_NOTIFY                        6
#define NFS_OP_CB_PUSHDELEG                     7
#define NFS_OP_CB_RECALLANY                     8
#define NFS_OP_CB_RECALLOBJAVAIL                9
#define NFS_OP_CB_RECALLSLOT                    10
#define NFS_OP_CB_SEQUENCE                      11
#define NFS_OP_CB_WANTCANCELLED                 12
#define NFS_OP_CB_NOTIFYLOCK                    13
#define NFS_OP_CB_NOTIFYDEVID                   14

/* NFSv4.1 callback opcodes */
#define NFS_V41_OP_CB_COUNT                     15

/* illegal op code */
#define NFS_OP_CB_ILLEGAL                       10044

/* NFSv4 file handle type flags */
#define NFS_FH_PERSISTENT                       0x00000000
#define NFS_FH_NOEXPIRE_WITH_OPEN               0x00000001
#define NFS_FH_VOLATILE_ANY                     0x00000002
#define NFS_FH_VOL_MIGRATION                    0x00000004
#define NFS_FH_VOL_RENAME                       0x00000008

/* NFSv4.1 Constants */

/* BIND_CONN_TO_SESSION - Associate Connection with Session */
#define NFS_CDFC4_FORE                          0x1
#define NFS_CDFC4_BACK                          0x2
#define NFS_CDFC4_FORE_OR_BOTH                  0x3
#define NFS_CDFC4_BACK_OR_BOTH                  0x7

#define NFS_CDFS4_FORE                          0x1
#define NFS_CDFS4_BACK                          0x2
#define NFS_CDFS4_BOTH                          0x3

/* EXCHANGE_ID - Instantiate Client ID */
#define NFS_EXCHGID4_FLAG_SUPP_MOVED_REFER       0x00000001
#define NFS_EXCHGID4_FLAG_SUPP_MOVED_MIGR        0x00000002
#define NFS_EXCHGID4_FLAG_BIND_PRINC_STATEID     0x00000100
#define NFS_EXCHGID4_FLAG_USE_NON_PNFS           0x00010000
#define NFS_EXCHGID4_FLAG_USE_PNFS_MDS           0x00020000
#define NFS_EXCHGID4_FLAG_USE_PNFS_DS            0x00040000
#define NFS_EXCHGID4_FLAG_MASK_PNFS              0x00070000
#define NFS_EXCHGID4_FLAG_UPD_CONFIRMED_REC_A    0x40000000
#define NFS_EXCHGID4_FLAG_CONFIRMED_R            0x80000000

#define NFS_EXCHGID4_SP4_NONE                    0
#define NFS_EXCHGID4_SP4_MACH_CRED               1
#define NFS_EXCHGID4_SP4_SSV                     2

/* CREATE_SESSION - Create New Session and Confirm Client ID */
#define NFS_CREATE_SESSION4_FLAG_PERSIST         0x00000001
#define NFS_CREATE_SESSION4_FLAG_CONN_BACK_CHAN  0x00000002
#define NFS_CREATE_SESSION4_FLAG_CONN_RDMA       0x00000004

/* GET_DIR_DELEGATION - Get a Directory Delegation */
#define NFS_GDD4_OK                              0
#define NFS_GDD4_UNAVAIL                         1

/* LAYOUTRETURN - Release Layout Information */
#define NFS_LAYOUT4_RET_REC_FILE                 1
#define NFS_LAYOUT4_RET_REC_FSID                 2
#define NFS_LAYOUT4_RET_REC_ALL                  3

#define NFS_LAYOUTRETURN4_FILE                   NFS_LAYOUT4_RET_REC_FILE
#define NFS_LAYOUTRETURN4_FSID                   NFS_LAYOUT4_RET_REC_FSID
#define NFS_LAYOUTRETURN4_ALL                    NFS_LAYOUT4_RET_REC_ALL

/* SECINFO_NO_NAME - Get Security on Unnamed Object */
#define NFS_SECINFO_STYLE4_CURRENT_FH            0
#define NFS_SECINFO_STYLE4_PARENT                1

/* SEQUENCE - Supply Per-Procedure Sequencing and Control */
#define NFS_SEQ4_STATUS_CB_PATH_DOWN                   0x00000001
#define NFS_SEQ4_STATUS_CB_GSS_CONTEXTS_EXPIRING       0x00000002
#define NFS_SEQ4_STATUS_CB_GSS_CONTEXTS_EXPIRED        0x00000004
#define NFS_SEQ4_STATUS_EXPIRED_ALL_STATE_REVOKED      0x00000008
#define NFS_SEQ4_STATUS_EXPIRED_SOME_STATE_REVOKED     0x00000010
#define NFS_SEQ4_STATUS_ADMIN_STATE_REVOKED            0x00000020
#define NFS_SEQ4_STATUS_RECALLABLE_STATE_REVOKED       0x00000040
#define NFS_SEQ4_STATUS_LEASE_MOVED                    0x00000080
#define NFS_SEQ4_STATUS_RESTART_RECLAIM_NEEDED         0x00000100
#define NFS_SEQ4_STATUS_CB_PATH_DOWN_SESSION           0x00000200
#define NFS_SEQ4_STATUS_BACKCHANNEL_FAULT              0x00000400
#define NFS_SEQ4_STATUS_DEVID_CHANGED                  0x00000800
#define NFS_SEQ4_STATUS_DEVID_DELETED                  0x00001000

/* CB_LAYOUTRECALL - Recall Layout from Client */
#define NFS_LAYOUTRECALL4_FILE NFS_LAYOUT4_RET_REC_FILE
#define NFS_LAYOUTRECALL4_FSID NFS_LAYOUT4_RET_REC_FSID
#define NFS_LAYOUTRECALL4_ALL  NFS_LAYOUT4_RET_REC_ALL

/* CB_NOTIFY - Notify Client of Directory Changes */
#define NFS_NOTIFY4_CHANGE_CHILD_ATTRS        0
#define NFS_NOTIFY4_CHANGE_DIR_ATTRS          1
#define NFS_NOTIFY4_REMOVE_ENTRY              2
#define NFS_NOTIFY4_ADD_ENTRY                 3
#define NFS_NOTIFY4_RENAME_ENTRY              4
#define NFS_NOTIFY4_CHANGE_COOKIE_VERIFIER    5

/* CB_RECALL_ANY - Keep Any N Recallable Objects */
#define NFS_RCA4_TYPE_MASK_RDATA_DLG          0
#define NFS_RCA4_TYPE_MASK_WDATA_DLG          1
#define NFS_RCA4_TYPE_MASK_DIR_DLG            2
#define NFS_RCA4_TYPE_MASK_FILE_LAYOUT        3
#define NFS_RCA4_TYPE_MASK_BLK_LAYOUT         4
#define NFS_RCA4_TYPE_MASK_OBJ_LAYOUT_MIN     8
#define NFS_RCA4_TYPE_MASK_OBJ_LAYOUT_MAX     9
#define NFS_RCA4_TYPE_MASK_OTHER_LAYOUT_MIN   12
#define NFS_RCA4_TYPE_MASK_OTHER_LAYOUT_MAX   15

/* CB_NOTIFY_DEVICEID - Notify Client of Device ID Changes */
#define NFS_NOTIFY_DEVICEID4_CHANGE           1
#define NFS_NOTIFY_DEVICEID4_DELETE           2

/*
 * NFSv4 ACL constants
 */
/* ACE support mask bits */
#define NFS_ACL_SUPPORT_ALLOW_ACL               0x00000001
#define NFS_ACL_SUPPORT_DENY_ACL                0x00000002
#define NFS_ACL_SUPPORT_AUDIT_ACL               0x00000004
#define NFS_ACL_SUPPORT_ALARM_ACL               0x00000008

/* ACE types */
#define NFS_ACE_ACCESS_ALLOWED_ACE_TYPE         0x00000000
#define NFS_ACE_ACCESS_DENIED_ACE_TYPE          0x00000001
#define NFS_ACE_SYSTEM_AUDIT_ACE_TYPE           0x00000002
#define NFS_ACE_SYSTEM_ALARM_ACE_TYPE           0x00000003

/* ACE flags */
#define NFS_ACE_FILE_INHERIT_ACE                0x00000001
#define NFS_ACE_DIRECTORY_INHERIT_ACE           0x00000002
#define NFS_ACE_NO_PROPAGATE_INHERIT_ACE        0x00000004
#define NFS_ACE_INHERIT_ONLY_ACE                0x00000008
#define NFS_ACE_SUCCESSFUL_ACCESS_ACE_FLAG      0x00000010
#define NFS_ACE_FAILED_ACCESS_ACE_FLAG          0x00000020
#define NFS_ACE_IDENTIFIER_GROUP                0x00000040
#define NFS_ACE_INHERITED_ACE                   0x00000080

/* ACE mask flags */
#define NFS_ACE_READ_DATA                       0x00000001
#define NFS_ACE_LIST_DIRECTORY                  0x00000001
#define NFS_ACE_WRITE_DATA                      0x00000002
#define NFS_ACE_ADD_FILE                        0x00000002
#define NFS_ACE_APPEND_DATA                     0x00000004
#define NFS_ACE_ADD_SUBDIRECTORY                0x00000004
#define NFS_ACE_READ_NAMED_ATTRS                0x00000008
#define NFS_ACE_WRITE_NAMED_ATTRS               0x00000010
#define NFS_ACE_EXECUTE                         0x00000020
#define NFS_ACE_DELETE_CHILD                    0x00000040
#define NFS_ACE_READ_ATTRIBUTES                 0x00000080
#define NFS_ACE_WRITE_ATTRIBUTES                0x00000100
#define NFS_ACE_WRITE_RETENTION                 0x00000200
#define NFS_ACE_WRITE_RETENTION_HOLD            0x00000400
#define NFS_ACE_DELETE                          0x00010000
#define NFS_ACE_READ_ACL                        0x00020000
#define NFS_ACE_WRITE_ACL                       0x00040000
#define NFS_ACE_WRITE_OWNER                     0x00080000
#define NFS_ACE_SYNCHRONIZE                     0x00100000
#define NFS_ACE_GENERIC_READ                    0x00120081
#define NFS_ACE_GENERIC_WRITE                   0x00160106
#define NFS_ACE_GENERIC_EXECUTE                 0x001200A0

/* deviceid4 */
#define NFS4_DEVICEID4_SIZE           16

/* retention_get */
#define NFS_RET4_DURATION_INFINITE    0xffffffffffffffff

/* na41_flag */
#define NFS_ACL4_AUTO_INHERIT         0x00000001
#define NFS_ACL4_PROTECTED            0x00000002
#define NFS_ACL4_DEFAULTED            0x00000004

/* thi_hintset */
#define NFS_TH4_READ_SIZE             0
#define NFS_TH4_WRITE_SIZE            1
#define NFS_TH4_READ_IOSIZE           2
#define NFS_TH4_WRITE_IOSIZE          3

/* fs_locations_info4 */
#define NFS_FSLI4BX_GFLAGS            0
#define NFS_FSLI4BX_TFLAGS            1

#define NFS_FSLI4BX_CLSIMUL           2
#define NFS_FSLI4BX_CLHANDLE          3
#define NFS_FSLI4BX_CLFILEID          4
#define NFS_FSLI4BX_CLWRITEVER        5
#define NFS_FSLI4BX_CLCHANGE          6
#define NFS_FSLI4BX_CLREADDIR         7

#define NFS_FSLI4BX_READRANK          8
#define NFS_FSLI4BX_WRITERANK         9
#define NFS_FSLI4BX_READORDER         10
#define NFS_FSLI4BX_WRITEORDER        11

#define NFS_FSLI4GF_WRITABLE          0x01
#define NFS_FSLI4GF_CUR_REQ           0x02
#define NFS_FSLI4GF_ABSENT            0x04
#define NFS_FSLI4GF_GOING             0x08
#define NFS_FSLI4GF_SPLIT             0x10

#define NFS_FSLI4TF_RDMA              0x01 /* Bits defined within the transport flag byte */

#define NFS_FSLI4IF_VAR_SUB           0x00000001 /* Flag bits in fli_flags */

/* layouttype4 */
#define NFS_LAYOUT4_NFSV4_1_FILES     0x1
#define NFS_LAYOUT4_OSD2_OBJECTS      0x2
#define NFS_LAYOUT4_BLOCK_VOLUME      0x3

/* layoutiomode4 */
#define NFS_LAYOUTIOMODE4_READ        1
#define NFS_LAYOUTIOMODE4_RW          2
#define NFS_LAYOUTIOMODE4_ANY         3

/* fs4_status_type */
#define NFS_STATUS4_FIXED             1
#define NFS_STATUS4_UPDATED           2
#define NFS_STATUS4_VERSIONED         3
#define NFS_STATUS4_WRITABLE          4
#define NFS_STATUS4_REFERRAL          5

/* nfsv4_1_file_layouthint4 */
#define NFS_NFL4_UFLG_MASK                   0x0000003F
#define NFS_NFL4_UFLG_DENSE                  0x00000001
#define NFS_NFL4_UFLG_COMMIT_THRU_MDS        0x00000002
#define NFS_NFL4_UFLG_STRIPE_UNIT_SIZE_MASK  0xFFFFFFC0

/* filelayout_hint_care4 */
#define NFS_NFLH4_CARE_DENSE                  NFS_NFL4_UFLG_DENSE
#define NFS_NFLH4_CARE_COMMIT_THRU_MDS        NFS_NFL4_UFLG_COMMIT_THRU_MDS
#define NFS_NFLH4_CARE_STRIPE_UNIT_SIZE       0x00000040
#define NFS_NFLH4_CARE_STRIPE_COUNT           0x00000080

/* fs_charset_cap4 */
#define NFS_FSCHARSET_CAP4_CONTAINS_NON_UTF8  0x1
#define NFS_FSCHARSET_CAP4_ALLOWS_ONLY_UTF8   0x2

/* ssv_subkey4 */
#define NFS_SSV4_SUBKEY_MIC_I2T                1
#define NFS_SSV4_SUBKEY_MIC_T2I                2
#define NFS_SSV4_SUBKEY_SEAL_I2T               3
#define NFS_SSV4_SUBKEY_SEAL_T2I               4

/*
 * Quads are defined as arrays of 2 32-bit values to ensure dense packing
 * for the protocol and to facilitate xdr conversion.
 */
struct nfs_uquad {
	u_int32_t       nfsuquad[2];
};
typedef struct nfs_uquad        nfsuint64;

/*
 * Used to convert between two u_int32_ts and a u_quad_t.
 */
union nfs_quadconvert {
	u_int32_t               lval[2];
	u_quad_t        qval;
};
typedef union nfs_quadconvert   nfsquad_t;

/*
 * special data/attribute associated with NFBLK/NFCHR
 */
struct nfs_specdata {
	uint32_t specdata1;     /* major device number */
	uint32_t specdata2;     /* minor device number */
};
typedef struct nfs_specdata nfs_specdata;

/*
 * an "fsid" large enough to hold an NFSv4 fsid.
 */
struct nfs_fsid {
	uint64_t major;
	uint64_t minor;
};
typedef struct nfs_fsid nfs_fsid;

/*
 * NFSv4 stateid structure
 */
struct nfs_stateid {
	uint32_t        seqid;
	uint32_t        other[3];
};
typedef struct nfs_stateid nfs_stateid;

#endif /* __APPLE_API_PRIVATE */
#endif /* _NFS_NFSPROTO_H_ */
