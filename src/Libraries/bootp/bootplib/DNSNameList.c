/*
 * Copyright (c) 2005-2018 Apple Inc. All rights reserved.
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

/*
 * DNSNameList.c
 * - convert a list of DNS domain names to and from the encodings
 *   described in RFC 1035
 * - contains DNSNameListBufferCreate() and DNSNameListCreate()
 */

/* 
 * Modification History
 *
 * January 4, 2006	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <mach/boolean.h>
#include <CoreFoundation/CFString.h>
#include "symbol_scope.h"
#include "DNSNameList.h"
#include "nbo.h"
#include "cfutil.h"

#ifdef TEST_DNSNAMELIST
#include "util.h"
#endif /* TEST_DNSNAMELIST */

#define DNS_PTR_PATTERN_BYTE_MASK	(uint8_t)0xc0
#define DNS_PTR_PATTERN_MASK		(uint16_t)0xc000
#define DNS_PTR_VALUE_MASK		(uint16_t)(~DNS_PTR_PATTERN_MASK)

#define DNS_LABEL_LENGTH_MAX		63

#define DNS_BUF_INITIAL_SIZE		128
typedef struct {
    uint8_t	db_buf_s[DNS_BUF_INITIAL_SIZE];
    bool	db_buf_user_supplied;
    uint8_t *	db_buf;
    int		db_buf_used;
    int		db_buf_size;
} DNSBuf, * DNSBufRef;

#define DNS_NAME_OFFSETS_N_START	8
typedef struct {
    uint32_t		dno_list_s[DNS_NAME_OFFSETS_N_START];
    uint32_t *		dno_list;
    int			dno_count;
    int			dno_size;
} DNSNameOffsets, * DNSNameOffsetsRef;

#define DNS_NAME_OFFSETS_LIST_N_START		8
typedef struct {
    DNSNameOffsetsRef * dnl_list;
    int			dnl_count;
    int			dnl_size;
} DNSNameOffsetsList, * DNSNameOffsetsListRef;

typedef struct {
    DNSNameOffsetsRef 	dn_offsets;
    uint8_t *		dn_buf;
    int			dn_buf_size;
} DNSName, * DNSNameRef;

typedef struct {
    DNSBuf		dnb_buf;
    DNSNameOffsetsList	dnb_names;
    Boolean		dnb_compact;
} DNSNamesBuf, * DNSNamesBufRef;

/**
 ** DNSBuf
 **/

STATIC void
DNSBufInit(DNSBufRef db, uint8_t * buf, int buf_size)
{
    bzero(db, sizeof(*db));
    if (buf != NULL) {
	db->db_buf_user_supplied = TRUE;
	db->db_buf = buf;
	db->db_buf_size = buf_size;
    }
    else {
	db->db_buf = db->db_buf_s;
	db->db_buf_size = sizeof(db->db_buf_s);
    }
    return;
}

STATIC void
DNSBufFreeElements(DNSBufRef db)
{
    if (db->db_buf_user_supplied == FALSE) {
	if (db->db_buf != NULL && db->db_buf != db->db_buf_s) {
	    free(db->db_buf);
	}
    }
    bzero(db, sizeof(*db));
    return;
}

STATIC __inline__ void
DNSBufSetUsed(DNSBufRef db, int used)
{
    if (db->db_buf_used < 0 || db->db_buf_used > db->db_buf_size) {
	fprintf(stderr, "trying to set used to %d\n", used);
	return;
    }
    db->db_buf_used = used;
    return;
}

STATIC __inline__ int
DNSBufUsed(DNSBufRef db)
{
    return (db->db_buf_used);
}

STATIC __inline__ uint8_t *
DNSBufBuffer(DNSBufRef db)
{
    return (db->db_buf);
}

STATIC bool
DNSBufAddData(DNSBufRef db, const void * data, int data_len)
{
#ifdef DEBUG
    printf("Adding: ");
    print_data((void *)data, data_len);
#endif /* DEBUG */

    if (data_len > (db->db_buf_size - db->db_buf_used)) {
	if (db->db_buf_user_supplied) {
	    fprintf(stderr, 
		    "user-supplied buffer failed to add data with"
		    " length %d (> %d)\n",
		    data_len, (db->db_buf_size - db->db_buf_used));
	    return (FALSE);
	}
#ifdef DEBUG
	printf("Buffer growing from %d to %d\n", db->db_buf_size,
	       db->db_buf_size + ((data_len > DNS_BUF_INITIAL_SIZE) 
	       ? data_len : DNS_BUF_INITIAL_SIZE));
#endif /* DEBUG */
	db->db_buf_size += (data_len > DNS_BUF_INITIAL_SIZE) 
	    ? data_len : DNS_BUF_INITIAL_SIZE;
	if (db->db_buf == db->db_buf_s) {
	    db->db_buf = malloc(db->db_buf_size);
	    memcpy(db->db_buf, db->db_buf_s, db->db_buf_used);
	}
	else {
	    db->db_buf = reallocf(db->db_buf, db->db_buf_size);
	}
    }
    memcpy(db->db_buf + db->db_buf_used, data, data_len);
    db->db_buf_used += data_len;
    return (TRUE);
}

/**
 ** DNSNameOffsets
 **/
STATIC void
DNSNameOffsetsInit(DNSNameOffsetsRef list)
{
    bzero(list, sizeof(*list));
    list->dno_size = DNS_NAME_OFFSETS_N_START;
    list->dno_list = list->dno_list_s;
    return;
}

STATIC DNSNameOffsetsRef
DNSNameOffsetsCreate(void)
{
    DNSNameOffsetsRef	list;

    list = malloc(sizeof(*list));
    if (list != NULL) {
	DNSNameOffsetsInit(list);
    }
    return (list);
}

STATIC bool
DNSNameOffsetsContainsOffset(DNSNameOffsetsRef list, uint32_t off)
{
    int		i;

    for (i = 0; i < list->dno_count; i++) {
	if (list->dno_list[i] == off) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

STATIC void
DNSNameOffsetsFreeElements(DNSNameOffsetsRef list)
{
    if (list->dno_list != NULL && list->dno_list != list->dno_list_s) {
	free(list->dno_list);
    }
    DNSNameOffsetsInit(list);
    return;
}

STATIC void
DNSNameOffsetsFree(DNSNameOffsetsRef * list_p)
{
    DNSNameOffsetsRef list;
    if (list_p == NULL) {
	return;
    }
    list = *list_p;
    if (list == NULL) {
	return;
    }
    if (list->dno_list != NULL && list->dno_list != list->dno_list_s) {
	free(list->dno_list);
    }
    free(list);
    *list_p = NULL;
    return;
}

STATIC void
DNSNameOffsetsAdd(DNSNameOffsetsRef list, uint32_t this_offset)
{
    if (list->dno_size == list->dno_count) {
#ifdef DEBUG
	printf("Growing from %d to %d\n", list->dno_size, list->dno_size * 2);
#endif /* DEBUG */
	list->dno_size *= 2;
	if (list->dno_list == list->dno_list_s) {
	    list->dno_list = malloc(list->dno_size * sizeof(*list->dno_list));
	    bcopy(list->dno_list_s, list->dno_list, 
		  list->dno_count * sizeof(*list->dno_list));
	}
	else {
	    list->dno_list = reallocf(list->dno_list, 
				      list->dno_size * sizeof(*list->dno_list));
	}
    }
    list->dno_list[list->dno_count++] = this_offset;
    return;
}

STATIC void
DNSNameOffsetsSet(DNSNameOffsetsRef list, int i, uint32_t this_offset)
{
    if (i > list->dno_count) {
	fprintf(stderr, "attempt to set offset 0x%x at index %d\n",
		this_offset, i);
    }
    list->dno_list[i] = this_offset;
    return;
}

STATIC __inline__ uint32_t
DNSNameOffsetsElement(const DNSNameOffsetsRef list, int i)
{
    return (list->dno_list[i]);
}

STATIC __inline__ uint32_t
DNSNameOffsetsCount(const DNSNameOffsetsRef list)
{
    return (list->dno_count);
}

/**
 ** DNSNameOffsetsList
 **/
STATIC void
DNSNameOffsetsListInit(DNSNameOffsetsListRef nlist)
{
    bzero(nlist, sizeof(*nlist));
    return;
}

STATIC void
DNSNameOffsetsListFreeElements(DNSNameOffsetsListRef nlist)
{
    if (nlist->dnl_list != NULL) {
	int	i;
	for (i = 0; i < nlist->dnl_count; i++) {
	    DNSNameOffsetsFree(nlist->dnl_list + i);
	}
	free(nlist->dnl_list);
    }
    DNSNameOffsetsListInit(nlist);
    return;
}

STATIC void
DNSNameOffsetsListAdd(DNSNameOffsetsListRef nlist, DNSNameOffsetsRef this_list)
{
    if (nlist->dnl_size == nlist->dnl_count) {
	if (nlist->dnl_size == 0) {
#ifdef DEBUG
	    printf("Namelist growing from 0 to %d\n", 
		   DNS_NAME_OFFSETS_LIST_N_START);
#endif /* DEBUG */
	    nlist->dnl_size = DNS_NAME_OFFSETS_LIST_N_START;
	    nlist->dnl_list 
		= malloc(nlist->dnl_size * sizeof(*nlist->dnl_list));
	}
	else {
#ifdef DEBUG
	    printf("Namelist growing from %d to %d\n", nlist->dnl_size,
		   nlist->dnl_size * 2);
#endif /* DEBUG */
	    nlist->dnl_size *= 2;
	    nlist->dnl_list 
		= reallocf(nlist->dnl_list, 
			   nlist->dnl_size * sizeof(*nlist->dnl_list));
	}
    }
    nlist->dnl_list[nlist->dnl_count++] = this_list;
    return;
}

STATIC __inline__ DNSNameOffsetsRef
DNSNameOffsetsListElement(DNSNameOffsetsListRef nlist, int i)
{
    return (nlist->dnl_list[i]);
}

STATIC __inline__ int
DNSNameOffsetsListCount(DNSNameOffsetsListRef nlist)
{
    return (nlist->dnl_count);
}

/**
 ** DNSName
 **/
STATIC void
DNSNameFree(DNSNameRef * name_p)
{
    DNSNameRef	name;
    
    if (name_p == NULL) {
	return;
    }
    name = *name_p;
    if (name == NULL) {
	return;
    }
    DNSNameOffsetsFree(&name->dn_offsets);
    free(name);
    *name_p = NULL;
}

STATIC DNSNameRef
DNSNameCreate(const char * dns_name)
{
    int			digits = 0;
    int			i;
    int			name_len;
    DNSNameRef		new_name;
    int			size = 0;
    uint32_t		start_offset;

    name_len = (int)strlen(dns_name);
    new_name = (DNSNameRef)malloc(sizeof(*new_name) + name_len + 2);
    new_name->dn_offsets = DNSNameOffsetsCreate();
    new_name->dn_buf = (uint8_t *)(new_name + 1);
    start_offset = 0;
    for (i = 0; i <= name_len; i++) {
	if (i == name_len || dns_name[i] == '.') {
	    if (digits == 0) {
		fprintf(stderr, "label length is 0\n");
		goto failed;
	    }
	    if (digits > DNS_LABEL_LENGTH_MAX) {
		fprintf(stderr, "label length %d > %d\n", digits,
			DNS_LABEL_LENGTH_MAX);
		goto failed;
	    }
	    DNSNameOffsetsAdd(new_name->dn_offsets, start_offset);
	    new_name->dn_buf[start_offset] = digits;
	    size += digits + 1;
	    digits = 0;
	    start_offset = i + 1;
	}
	else {
	    new_name->dn_buf[1 + i] = dns_name[i];
	    digits++;
	}
    }
    new_name->dn_buf_size = size + 1;
    new_name->dn_buf[size] = '\0';
    return (new_name);
 failed:
    DNSNameFree(&new_name);
    return (NULL);
}

STATIC int
DNSNameMatch(DNSNameRef name1, DNSNameRef name2)
{
    int		count1 = DNSNameOffsetsCount(name1->dn_offsets);
    int		count2 = DNSNameOffsetsCount(name2->dn_offsets);
    int		i1;
    int		i2;
    int		matched = 0;

    for (i1 = (count1 - 1), i2 = (count2 - 1);
	 i1 >= 0 && i2 >= 0;
	 i1--, i2--) {
	uint32_t	buf_off1 = DNSNameOffsetsElement(name1->dn_offsets, i1);
	uint32_t	buf_off2 = DNSNameOffsetsElement(name2->dn_offsets, i2);
	uint8_t		nlen1 = *(uint8_t *)(name1->dn_buf + buf_off1);
	uint8_t		nlen2 = *(uint8_t *)(name2->dn_buf + buf_off2);

	if (nlen1 != nlen2) {
	    break;
	}
#ifdef DEBUG
	printf("Comparing %.*s to %.*s\n",
	       nlen1, name1->dn_buf + buf_off1 + 1, 
	       nlen1, name2->dn_buf + buf_off2 + 1);
#endif /* DEBUG */
	if (memcmp(name1->dn_buf + buf_off1 + 1, name2->dn_buf + buf_off2 + 1,
		   nlen1)) {
	    break;
	}
	matched++;
    }
    return (matched);
}

/**
 ** DNSNamesBuf
 **/

STATIC void
DNSNamesBufFreeElements(DNSNamesBufRef nb)
{
    DNSBufFreeElements(&nb->dnb_buf);
    DNSNameOffsetsListFreeElements(&nb->dnb_names);
    return;
}

STATIC void
DNSNamesBufInit(DNSNamesBufRef nb, void * buf, int buf_size, Boolean compact)
{
    DNSBufInit(&nb->dnb_buf, buf, buf_size);
    DNSNameOffsetsListInit(&nb->dnb_names);
    nb->dnb_compact = compact;
    return;
}

STATIC __inline__ int
DNSNamesBufUsed(DNSNamesBufRef nb)
{
    return (DNSBufUsed(&nb->dnb_buf));
}

STATIC __inline__ void
DNSNamesBufSetUsed(DNSNamesBufRef nb, int used)
{
    return (DNSBufSetUsed(&nb->dnb_buf, used));
}

STATIC __inline__ uint8_t *
DNSNamesBufBuffer(DNSNamesBufRef nb)
{
    return (DNSBufBuffer(&nb->dnb_buf));
}

STATIC bool
DNSNamesBufAddData(DNSNamesBufRef nb, const void * data, int data_len)
{
    return (DNSBufAddData(&nb->dnb_buf, data, data_len));
}

STATIC bool
DNSNamesBufAddName(DNSNamesBufRef nb, const char * dns_name)
{
    int			best_matched = 0;
    DNSNameOffsetsRef	best_name = NULL;
    int			best_start_offset = 0;
    uint32_t		buf_offset;
    int			count;
    int			i;
    DNSNameRef		new_name = NULL;
    bool		ret = FALSE;
    int			start_count;

    buf_offset = DNSNamesBufUsed(nb);

    /* parse the name into labels */
    new_name = DNSNameCreate(dns_name);
    if (new_name == NULL) {
	goto done;
    }
    if (!nb->dnb_compact) {
	/* add the whole encoded name to nb's buffer */
	if (DNSNamesBufAddData(nb, new_name->dn_buf,
			       new_name->dn_buf_size) == FALSE) {
	}
	else {
	    ret = TRUE;
	}
	goto done;
    }

    /* find the name's best match with those that are already in the list */
    for (i = 0; i < DNSNameOffsetsListCount(&nb->dnb_names); i++) {
	int			matched;
	DNSName			this_name;
	DNSNameOffsetsRef	this_offset;

	this_offset = DNSNameOffsetsListElement(&nb->dnb_names, i);
	if (this_offset == NULL) {
	    /* this can't happen */
	    break;
	}
	this_name.dn_buf = DNSNamesBufBuffer(nb);
	this_name.dn_offsets = this_offset;
	matched = DNSNameMatch(&this_name, new_name);
	if (matched > best_matched) {
	    best_matched = matched;
	    best_name = this_offset;
	}
    }
    if (best_name == NULL) {
	/* add the whole encoded name to nb's buffer */
	if (DNSNamesBufAddData(nb, new_name->dn_buf,
			       new_name->dn_buf_size) == FALSE) {
	    goto done;
	}
    }
    count = DNSNameOffsetsCount(new_name->dn_offsets);
    start_count = count - best_matched;
    if (best_name != NULL) {
	best_start_offset = DNSNameOffsetsCount(best_name) - best_matched;
    }
    for (i = 0; i < count; i++) {
	if (i >= start_count) {
	    uint32_t	best_offset;

	    best_offset = DNSNameOffsetsElement(best_name, best_start_offset++);
	    if (i == start_count) {
		/* insert a pointer to the best match at the right offset */
		uint16_t	ptr;
		
		ptr = htons(DNS_PTR_PATTERN_MASK | best_offset);
		if (DNSNamesBufAddData(nb, &ptr, sizeof(ptr)) == FALSE) {
		    goto done;
		}
	    }
	    /* change offset to point to the best match's offset */
	    DNSNameOffsetsSet(new_name->dn_offsets, i, best_offset);
	}
	else {
	    uint32_t	name_offset;

	    name_offset = DNSNameOffsetsElement(new_name->dn_offsets, i);
	    if (best_name != NULL) {
		/* add this component to nb's buffer */
		if (DNSNamesBufAddData(nb, new_name->dn_buf + name_offset,
				       new_name->dn_buf[name_offset] + 1)
		    == FALSE) {
		    goto done;
		}
	    }
	    /* change offset so that it's relative to nb's buffer */
	    DNSNameOffsetsSet(new_name->dn_offsets, i, 
			      name_offset + buf_offset);
	}
    }
    /* "steal" the offsets from the name before it's released */
    DNSNameOffsetsListAdd(&nb->dnb_names, new_name->dn_offsets);
    new_name->dn_offsets = NULL;
    ret = TRUE;

 done:
    if (ret == FALSE) {
	/* roll back */
	DNSNamesBufSetUsed(nb, buf_offset);
    }

    DNSNameFree(&new_name);
    return (ret);
}

/**
 ** DNSNameList* API's
 **/

/* 
 * Function: DNSNameListBufferCreate
 *   Convert the given list of DNS domain names into either of two formats
 *   described in RFC 1035.  If "buffer" is NULL, this routine allocates
 *   a buffer of sufficient size and returns its size in "buffer_size".
 *   Use free() to release the memory.
 *
 *   If "buffer" is not NULL, this routine places at most "buffer_size" 
 *   bytes into "buffer".  If "buffer" is too small, NULL is returned, and
 *   "buffer_size" reflects the number of bytes used in the partial conversion.
 *
 *   If "compact" is TRUE, generates the compact form (RFC 1035 section 4.1.4),
 *   otherwise generates the non-compact form (RFC 1035 section 3.1).
 *   
 * Returns:
 *   NULL if the conversion failed, non-NULL otherwise.
 */
PRIVATE_EXTERN uint8_t *
DNSNameListBufferCreate(const char * names[], int names_count,
			uint8_t * buffer, int * buffer_size, Boolean compact)
{
    int			bufsize = *buffer_size;
    int			i;
    DNSNamesBuf		nb;
    int			used = 0;

    if (names_count == 0) {
	goto failed;
    }
    if (buffer == NULL) {
	bufsize = 0;
    }
    else if (bufsize == 0) {
	goto failed;
    }
    DNSNamesBufInit(&nb, buffer, bufsize, compact);
    for (i = 0; i < names_count; i++) {
	if (DNSNamesBufAddName(&nb, names[i]) == FALSE) {
	    fprintf(stderr, "failed to add %s\n", names[i]);
	    if (buffer != NULL) {
		used = DNSNamesBufUsed(&nb);
		buffer = NULL;
	    }
	    DNSNamesBufFreeElements(&nb);
	    goto failed;
	}
    }
    used = DNSNamesBufUsed(&nb);
    if (buffer == NULL) {
	/* make a copy of just the buffer and return it */
	buffer = malloc(used);
	memcpy(buffer, DNSNamesBufBuffer(&nb), used);
    }
    DNSNamesBufFreeElements(&nb);

 failed:
    *buffer_size = used;
    return (buffer);

}

/* 
 * Function: DNSNameListDataCreateWithArray
 *
 * Purpose:
 */
PRIVATE_EXTERN CFDataRef
DNSNameListDataCreateWithArray(CFArrayRef list, Boolean compact)
{
    CFDataRef	data = NULL;
    uint8_t *	encoded;
    int		encoded_length;
    char * *	strlist;
    int		strlist_count;

    strlist = my_CStringArrayCreate(list, &strlist_count);
    if (strlist != NULL) {
	encoded = DNSNameListBufferCreate((const char * *)strlist,
					  strlist_count,
					  NULL, &encoded_length,
					  compact);
	free(strlist);
	data = CFDataCreate(NULL, encoded, encoded_length);
	free(encoded);
    }
    return (data);
}

/* 
 * Function: DNSNameListDataCreateWithString
 *
 * Purpose:
 */
PRIVATE_EXTERN CFDataRef
DNSNameListDataCreateWithString(CFStringRef cfstr)
{
    CFDataRef	data = NULL;
    uint8_t *	encoded;
    int		encoded_length;
    char *	str;

    str = my_CFStringToCString(cfstr, kCFStringEncodingUTF8);
    if (str == NULL) {
	goto done;
    }
    encoded = DNSNameListBufferCreate((const char * *)&str, 1, 
				      NULL, &encoded_length,
				      FALSE);
    free(str);
    if (encoded == NULL) {
	goto done;
    }
    data = CFDataCreate(NULL, encoded, encoded_length);
    free(encoded);

 done:
    return (data);
}

/*
 * Function: DNSNameListCreateCommon
 *
 * Purpose:
 *   Convert the domain name list form described in RFC 1035 to a list
 *   of domain names.
 * 
 * Returns:
 *   The number of names, and DNSBuf is filled with name strings if successful,
 *   0 otherwise.
 *
 *   If a non-zero return value is returned, the caller is responsible for
 *   calling DNSBufFreeElements() on the DNSBuf.
 *
 * Notes:
 *   This routine processes the encoded buffer, keeping track of the
 *   "read_head", and the "scan" point, the farthest we've reached in the
 *   buffer.  We look for either pointers, labels, or end labels.  
 *
 *   A pointer is checked to ensure that:
 *   - it is not truncated
 *   - it does not point past the last completed name
 *   - it points to the start of a label
 *   - it does not create an infinite loop
 *
 *   The label is checked to ensure that it is not truncated, and that it
 *   is not too long (<= 63 bytes).  When an end label is encountered, the
 *   domain name is complete, and the "read_head" advances to the "scan" point.
 */
STATIC int
DNSNameListCreateCommon(const uint8_t * buffer, int buffer_size,
			DNSBufRef name_buf_p)
{
    DNSNameOffsets	buffer_offsets;
    bool		first;
    uint32_t		last_completed_name = 0;
    uint32_t		left;
    int			list_count = 0;
    uint32_t		read_head;
    uint32_t		scan;
    bool		success = FALSE;

    if (buffer == NULL || buffer_size == 0) {
	return (0);
    }
    DNSNameOffsetsInit(&buffer_offsets);
    left = buffer_size;
    first = TRUE;
    for (read_head = scan = 0; read_head < buffer_size; ) {
	uint16_t	ptr;

	if ((buffer[read_head] & DNS_PTR_PATTERN_BYTE_MASK) 
	    == DNS_PTR_PATTERN_BYTE_MASK) {
	    /* got a pointer, validate it */
	    if (read_head >= scan) {
		if (left < 2) {
		    fprintf(stderr, "truncated pointer value\n");
		    goto failed;
		}
		scan += 2;
		left -= 2;
	    }

	    ptr = (net_uint16_get(buffer + read_head))
			     & DNS_PTR_VALUE_MASK;
	    if (ptr >= read_head) {
		fprintf(stderr,
			"pointer points at or ahead of current position\n");
		goto failed;
	    }
	    if (last_completed_name == 0
		|| ptr >= last_completed_name) {
		fprintf(stderr, "attempt to create infinite loop\n");
		goto failed;
	    }
	    if (DNSNameOffsetsContainsOffset(&buffer_offsets, ptr) == FALSE) {
		fprintf(stderr, "attempt to point off into the weeds\n");
		goto failed;
	    }
	    /* it's a good pointer, so follow it */
	    read_head = ptr;
	}
	else {
	    uint8_t	len = buffer[read_head];
	    
	    if (read_head >= scan) {
		if (len > DNS_LABEL_LENGTH_MAX) {
		    fprintf(stderr, "label length %d > %d\n", len,
			    DNS_LABEL_LENGTH_MAX);
		    goto failed;
		}
		if (left < (len + 1)) {
		    fprintf(stderr, "label truncated %d < %d\n",
			    left, len + 1);
		    goto failed;
		}
		scan += len + 1;
		left -= len + 1;
	    }
	    if (len == 0) {
		/* end label */
		char	ch = '\0';

		(void)DNSBufAddData(name_buf_p, &ch, 1);
		last_completed_name = scan;
		read_head = scan;
		list_count++;
		/* start on the next domain name */
		first = TRUE;
	    }
	    else {
		/* got a label */
		char	dot = '.';

		if (DNSNameOffsetsContainsOffset(&buffer_offsets, read_head) 
		    == FALSE) {
		    DNSNameOffsetsAdd(&buffer_offsets, read_head);
		}
		if (first) {
		    /* got the start of a new domain name */
		    first = FALSE;
		}
		else {
		    (void)DNSBufAddData(name_buf_p, &dot, 1);
		}
		/* add this label to the buffer */
		DNSBufAddData(name_buf_p, buffer + read_head + 1, len);
		read_head += len + 1;
	    }
	}
    }

    if (list_count == 0) {
	if (DNSBufUsed(name_buf_p) == 0) {
	    fprintf(stderr, "empty list\n");
	}
	else {
	    fprintf(stderr, "name missing end label\n");
	}
    }
    else {
	success = TRUE;
    }

 failed:
    if (!success) {
	list_count = 0;
	DNSBufFreeElements(name_buf_p);
    }
    DNSNameOffsetsFreeElements(&buffer_offsets);
    return (list_count);
}

/*
 * Function: DNSNameListCreate
 *
 * Purpose:
 *   Convert the domain name list form described in RFC 1035 to a list
 *   of domain names.
 *
 * Returns:
 *   A non-NULL malloc'd list of DNS domain names, and the non-zero count of
 *   elements in the list if the conversion was successful.
 *   NULL if an error occurred i.e. the supplied buffer contained an invalid
 *   encoding.
 *
 * Notes:
 *   When processing is complete, the pointer array and name buffer are
 *   allocated in one contiguous chunk of memory.  The name buffer is copied
 *   to the newly allocated area, and the pointer array is populated by
 *   walking the name buffer looking for complete strings.
 */
PRIVATE_EXTERN const char * *
DNSNameListCreate(const uint8_t * buffer, int buffer_size, int * names_count)
{
    char * *		list = NULL;
    int			list_count;
    DNSBuf		name_buf;

    DNSBufInit(&name_buf, NULL, 0);
    list_count = DNSNameListCreateCommon(buffer, buffer_size, &name_buf);
    if (list_count == 0) {
	goto failed;
    }
    /* create single returned allocation: name pointers + name strings */
    list = (char * *)
	malloc(DNSBufUsed(&name_buf) + sizeof(*list) * list_count);
    {
	int 		i;
	char *		name_start;

	name_start = (char *)(list) + sizeof(*list) * list_count;
	memcpy(name_start, DNSBufBuffer(&name_buf),
	       DNSBufUsed(&name_buf));
	for (i = 0; i < list_count; i++) {
	    list[i] = name_start;
	    name_start += strlen(name_start) + 1;
	}
    }
 failed:
    if (list != NULL) {
	*names_count = list_count;
    }
    else {
	*names_count = 0;
    }
    DNSBufFreeElements(&name_buf);
    return ((const char * *)list);
}

/*
 * Function: DNSNameListCreateArray
 * Purpose:
 *   Convert the compact domain name list form described in RFC 1035 to an
 *   CFArray of UTF8-encoded CFStrings corresponding to the domain search list.
 */
PRIVATE_EXTERN CFArrayRef
DNSNameListCreateArray(const uint8_t * buffer, int buffer_size)
{
    CFMutableArrayRef	array;
    int			list_count;
    int 		i;
    DNSBuf		name_buf;
    const char *	name_start;

    DNSBufInit(&name_buf, NULL, 0);
    list_count = DNSNameListCreateCommon(buffer, buffer_size,
					 &name_buf);
    if (list_count == 0) {
	return (NULL);
    }
    array = CFArrayCreateMutable(NULL, list_count,
				 &kCFTypeArrayCallBacks);
    name_start = (const char *)DNSBufBuffer(&name_buf);
    for (i = 0; i < list_count; i++) {
	CFStringRef str;

	str = CFStringCreateWithCString(NULL, name_start,
					kCFStringEncodingUTF8);
	if (str == NULL) {
#ifdef TEST_DNSNAMELIST
	    fprintf(stderr, "failed to convert '%s' to UTF8\n",
		    name_start);
#endif /* TEST_DNSNAMELIST */
	}
	else {
	    CFArrayAppendValue(array, str);
	    CFRelease(str);
	}
	name_start += strlen(name_start) + 1;
    }
    if (CFArrayGetCount(array) == 0) {
	CFRelease(array);
	array = NULL;
    }
    DNSBufFreeElements(&name_buf);
    return (array);
}

#ifdef TEST_DNSNAMELIST

const uint8_t	bad_buf1[] = {
    5, 'a', 'p', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0,
    3, 'g', 'o', 'o', 0xc0, 0xb };

const uint8_t	bad_buf2[] = {
    5, 'a', 'p', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0xc0, 0x00 };

const uint8_t	bad_buf3[] = {
    5, 'a', 'p', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0xc0, 0x0a };

const uint8_t	bad_buf4[] = {
    5, 'a', 'p', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0,
    3, 'g', 'o', 'o', 0xc0, 0x01 };

const uint8_t	bad_buf5[] = {
    5, 'a', 'p', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0,
    3, 'g', 'o', 'o', 0xc0 };

const uint8_t	bad_buf6[] = {
    5, 'a', 'p', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0,
    16, 'g', 'o', 'o' };

const uint8_t	bad_buf7[] = {
    5, 'a', 'p', 'p', 'l', 'e', 3, 'c', 'o', 'm'
};

const uint8_t	bad_buf8[] = {
    0x07, 0x07, 0x97, 0x10, 0x16, 0x01, 0x03, 0x97, 0x02, 0x10, 0x11, 0x00, 0x07, 0x07,
    0x97, 0x10, 0x16, 0x01, 0x03, 0x97, 0x03, 0x08, 0x97, 0x10, 0x00, 0x06, 0x97, 0x07, 0x15, 0x01,
    0x15, 0x15, 0x02, 0x10, 0x11, 0x00
};

const uint8_t good_buf1[] = {
	4, 'e', 'u', 'r', 'o', 5, 'a', 'p', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0, 
	9, 'm', 'a', 'r', 'k', 'e', 't', 'i', 'n', 'g', 0xc0, 0x0b,
	11, 'e', 'n', 'g', 'i', 'n', 'e', 'e', 'r', 'i', 'n', 'g', 0xc0, 0x05,
	
};

const uint8_t good_buf2[] = {
	1, 'z', 0, 
	1, 'y', 0xc0, 0x00,
	1, 'x', 0xc0, 0x03,
	1, 'w', 1, 'x', 0, 
	1, 'w', 0xc0, 0x07,
	1, 'w', 1, 'x', 1, 'y', 1, 'z', 0xc0, 0x0b,
	
};

struct test {
    const char *	name;
    const uint8_t *	buf;
    int			buf_size;
    bool		expect_success;
};

STATIC const struct test all_tests[] = {
    { "infinite loop", bad_buf1, sizeof(bad_buf1), FALSE },
    { "infinite loop", bad_buf2, sizeof(bad_buf2), FALSE },
    { "forward pointer", bad_buf3, sizeof(bad_buf3), FALSE },
    { "pointer to non-existent location", bad_buf4, sizeof(bad_buf4), FALSE },
    { "truncated pointer", bad_buf5, sizeof(bad_buf5), FALSE },
    { "truncated label", bad_buf6, sizeof(bad_buf6), FALSE },
    { "no end label", bad_buf7, sizeof(bad_buf7), FALSE },
    { "bad chars", bad_buf8, sizeof(bad_buf8), FALSE },
    { "apple.com", good_buf1, sizeof(good_buf1), TRUE },
    { "w.x", good_buf2, sizeof(good_buf2), TRUE },
    { NULL, NULL, 0}
};

#include <ctype.h>

STATIC void
DNSNameListBufferDumpCharArray(const uint8_t * buf, int buf_size)
{
    int			i;
    const uint8_t *	scan;

    printf("const uint8_t buf[] = {\n\t");
    for (i = 0, scan = buf; i < buf_size; scan++, i++) {
	uint8_t	val = *scan;
	if ((val & DNS_PTR_PATTERN_BYTE_MASK) == DNS_PTR_PATTERN_BYTE_MASK) {
	    if (i == (buf_size - 1)) {
		printf("truncated pointer!\n");
		return;
	    }
	    printf("0x%02x, 0x%02x,\n\t", scan[0], scan[1]);
	    /* skip the additional ptr byte */
	    scan++;
	    i++;
	}
	else {
	    if (val == 0) {
		printf("0, \n\t");
	    }
	    else if (isprint(val)) {
		printf("'%c', ", val);
	    }
	    else {
		printf("%d, ", val);
	    }
	}
    }
    printf("\n};\n");
    fflush(stdout);
    return;
}


STATIC void
test_domain_list(int argc, const char * argv[], Boolean compact)
{
    uint8_t *		buf;
    int			buf_size;
    int 		i;
    const char * *	list;
    int			list_count;
    uint8_t		tmp[16];

    buf = DNSNameListBufferCreate(argv + 1, argc - 1, NULL, &buf_size, compact);
    if (buf != NULL) {
	print_data(buf, buf_size);
	list = DNSNameListCreate(buf, buf_size, &list_count);
	if (list != NULL) {
	    printf("Domains%s: ", compact ? " [compact]" : "");
	    for (i = 0; i < list_count; i++) {
		if (strcasecmp(list[i], argv[i + 1]) != 0) {
		    fprintf(stderr, "ERROR! %s != %s\n",
			    list[i], argv[i + 1]);
		    break;
		}
		printf("%s%s", (i > 0) ? " " : "", list[i]);
	    }
	    printf("\n");
	    free(list);
	}
	DNSNameListBufferDumpCharArray(buf, buf_size);
	free(buf);
    }
    buf_size = sizeof(tmp);
    buf = DNSNameListBufferCreate(argv + 1, argc - 1, tmp, &buf_size, compact);
    if (buf_size == 0) {
	printf("FAILED to convert\n");
    }
    else {
	if (buf == NULL) {
	    printf("Buffer not large enough, but got %d bytes\n",
		   buf_size);
	}
	print_data(tmp, buf_size);
	list = DNSNameListCreate(tmp, buf_size, &list_count);
	if (list != NULL) {
	    printf("Domains%s: ", compact ? " [compact]" : "");
	    for (i = 0; i < list_count; i++) {
		printf("%s%s", (i > 0) ? " " : "", list[i]);
	    }
	    printf("\n");
	    free(list);
	}
    }
}

int
main(int argc, const char * argv[])
{
    int 		i;
    const char * *	list;
    int			list_count;
    const struct test *	test;

    if (argc >= 2) {
	test_domain_list(argc, argv, TRUE);
	test_domain_list(argc, argv, FALSE);
    }
    for (i = 0, test = all_tests; test->name != NULL; i++, test++) {
	CFArrayRef	array;

	list = DNSNameListCreate(test->buf, test->buf_size,
				 &list_count);
	array = DNSNameListCreateArray(test->buf, test->buf_size);
	printf("Test %d '%s' ", i + 1, test->name);
	if (test->expect_success
	    == (array != NULL && list != NULL)) {
	    printf("PASSED\n");
	}
	else {
	    printf("FAILED\n");
	    printf("Halting tests\n");
	    exit(2);
	}
	if (list != NULL) {
	    int j;

	    printf("Domains:%s ",
		   !test->expect_success ? "[BAD STRINGS]" : "");
	    for (j = 0; j < list_count; j++) {
		printf("%s%s", (j > 0) ? ", " : "", list[j]);
	    }
	    printf("\n");
	    free(list);
	}
	if (array != NULL) {
	    CFShow(array);
	    CFRelease(array);
	}

    }
    exit(0);
}
#endif /* TEST_DNSNAMELIST */
