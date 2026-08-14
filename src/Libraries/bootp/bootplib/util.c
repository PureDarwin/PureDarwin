/*
 * Copyright (c) 2003-2013 Apple Inc. All rights reserved.
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
 * util.c
 * - contains miscellaneous routines
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/syslog.h>
#include <errno.h>
#include <mach/boolean.h>
#include <string.h>
#include <ctype.h>
#include "util.h"
#include "cfutil.h"
#include "symbol_scope.h"

/* 
 * Function: nbits_host
 * Purpose:
 *   Return the number of bits of host address 
 */
PRIVATE_EXTERN int
nbits_host(struct in_addr mask)
{
    u_long l = iptohl(mask);
    int i;

    for (i = 0; i < 32; i++) {
	if (l & (1 << i))
	    return (32 - i);
    }
    return (32);
}

/*
 * Function: inet_nettoa
 * Purpose:
 *   Turns a network address (expressed as an IP address and mask)
 *   into a string e.g. 17.202.40.0 and mask 255.255.252.0 yields
 *   the string  "17.202.40/22".
 */
PRIVATE_EXTERN char *
inet_nettoa(struct in_addr addr, struct in_addr mask)
{
    uint8_t *		addr_p;
    int 		nbits = nbits_host(mask);
    int 		nbytes;
    static char 	sbuf[32];
    char 		tmp[8];

#define NBITS_PER_BYTE	8    
    sbuf[0] = '\0';
    nbytes = (nbits + NBITS_PER_BYTE - 1) / NBITS_PER_BYTE;
//    printf("-- nbits %d, nbytes %d--", nbits, nbytes);
    for (addr_p = (uint8_t *)&addr.s_addr; nbytes > 0; addr_p++) {

	snprintf(tmp, sizeof(tmp), "%d%s", *addr_p, nbytes > 1 ? "." : "");
	strlcat(sbuf, tmp, sizeof(sbuf));
	nbytes--;
    }
    if (nbits % NBITS_PER_BYTE) {
	snprintf(tmp, sizeof(tmp), "/%d", nbits);
	strlcat(sbuf, tmp, sizeof(sbuf));
    }
    return (sbuf);
}

/* 
 * Function: random_range
 * Purpose:
 *   Return a random number in the given range.
 */
PRIVATE_EXTERN long
random_range(long bottom, long top)
{
    long ret;
    long number = top - bottom + 1;
    long range_size = UINT32_MAX / number;
    if (range_size == 0)
	return (bottom);
    ret = (arc4random() / range_size) + bottom;
    return (ret);
}

/*
 * Function: timeval_subtract
 *
 * Purpose:
 *   Computes result = tv1 - tv2.
 */
PRIVATE_EXTERN void
timeval_subtract(struct timeval tv1, struct timeval tv2, 
		 struct timeval * result)
{
    result->tv_sec = tv1.tv_sec - tv2.tv_sec;
    result->tv_usec = tv1.tv_usec - tv2.tv_usec;
    if (result->tv_usec < 0) {
	result->tv_usec += USECS_PER_SEC;
	result->tv_sec--;
    }
    return;
}

/*
 * Function: timeval_add
 *
 * Purpose:
 *   Computes result = tv1 + tv2.
 */
PRIVATE_EXTERN void
timeval_add(struct timeval tv1, struct timeval tv2,
	    struct timeval * result)
{
    result->tv_sec = tv1.tv_sec + tv2.tv_sec;
    result->tv_usec = tv1.tv_usec + tv2.tv_usec;
    if (result->tv_usec > USECS_PER_SEC) {
	result->tv_usec -= USECS_PER_SEC;
	result->tv_sec++;
    }
    return;
}

/*
 * Function: timeval_compare
 *
 * Purpose:
 *   Compares two timeval values, tv1 and tv2.
 *
 * Returns:
 *   -1		if tv1 is less than tv2
 *   0 		if tv1 is equal to tv2
 *   1 		if tv1 is greater than tv2
 */
PRIVATE_EXTERN int
timeval_compare(struct timeval tv1, struct timeval tv2)
{
    struct timeval result;

    timeval_subtract(tv1, tv2, &result);
    if (result.tv_sec < 0 || result.tv_usec < 0)
	return (-1);
    if (result.tv_sec == 0 && result.tv_usec == 0)
	return (0);
    return (1);
}

/*
 * Function: print_data_cfstr
 * Purpose:
 *   Displays the buffer as a series of 8-bit hex numbers with an ASCII
 *   representation off to the side.
 */
PRIVATE_EXTERN void
print_data_cfstr(CFMutableStringRef str, const uint8_t * data_p,
		 int n_bytes)
{
#define CHARS_PER_LINE 	16
    char		line_buf[CHARS_PER_LINE + 1];
    int			line_pos;
    int			offset;

    for (line_pos = 0, offset = 0; offset < n_bytes; offset++, data_p++) {
	if (line_pos == 0) {
	    STRING_APPEND(str, "%04x ", offset);
	}

	line_buf[line_pos] = isprint(*data_p) ? *data_p : '.';
	STRING_APPEND(str, " %02x", *data_p);
	line_pos++;
	if (line_pos == CHARS_PER_LINE) {
	    line_buf[CHARS_PER_LINE] = '\0';
	    STRING_APPEND(str, "  %s\n", line_buf);
	    line_pos = 0;
	}
	else if (line_pos == (CHARS_PER_LINE / 2))
	    STRING_APPEND(str, " ");
    }
    if (line_pos) { /* need to finish up the line */
	char * extra_space = "";
	if (line_pos < (CHARS_PER_LINE / 2)) {
	    extra_space = " ";
	}
	for (; line_pos < CHARS_PER_LINE; line_pos++) {
	    STRING_APPEND(str, "   ");
	    line_buf[line_pos] = ' ';
	}
	line_buf[CHARS_PER_LINE] = '\0';
	STRING_APPEND(str, "  %s%s\n", extra_space, line_buf);
    }
    return;
}

PRIVATE_EXTERN void
fprint_data(FILE * out_f, const uint8_t * data_p, int n_bytes)
{
    CFMutableStringRef	str;

    str = CFStringCreateMutable(NULL, 0);
    print_data_cfstr(str, data_p, n_bytes);
    my_CFStringPrint(out_f, str);
    CFRelease(str);
    fflush(out_f);
    return;
}

PRIVATE_EXTERN void
print_data(const uint8_t * data_p, int n_bytes)
{
    fprint_data(stdout, data_p, n_bytes);
    return;
}

PRIVATE_EXTERN void
print_bytes_sep_cfstr(CFMutableStringRef str, uint8_t * data_p, int n_bytes,
		      char separator)
{
    int i;

    for (i = 0; i < n_bytes; i++) {
	char  	sep[3];

	if (i == 0) {
	    sep[0] = '\0';
	}
	else {
	    if ((i % 8) == 0 && separator == ' ') {
		sep[0] = sep[1] = ' ';
		sep[2] = '\0';
	    }
	    else {
		sep[0] = separator;
		sep[1] = '\0';
	    }
	}
	STRING_APPEND(str, "%s%02x", sep, data_p[i]);
    }
    return;
}

PRIVATE_EXTERN void
print_bytes_cfstr(CFMutableStringRef str, uint8_t * data, int len)
{
    print_bytes_sep_cfstr(str, data, len, ' ');
    return;
}


PRIVATE_EXTERN void
fprint_bytes_sep(FILE * out_f, uint8_t * data_p, int n_bytes, char separator)
{
    CFMutableStringRef	str;

    str = CFStringCreateMutable(NULL, 0);
    print_bytes_sep_cfstr(str, data_p, n_bytes, separator);
    my_CFStringPrint(out_f, str);
    CFRelease(str);
    fflush(out_f);
    return;
}

PRIVATE_EXTERN void
print_bytes(uint8_t * data, int len)
{
    fprint_bytes_sep(stdout, data, len, ' ');
    return;
}

PRIVATE_EXTERN void
print_bytes_sep(uint8_t * data, int len, char separator)
{
    fprint_bytes_sep(stdout, data, len, separator);
    return;
}


/*
 * Function: create_path
 *
 * Purpose:
 *   Create the given directory hierarchy.  Return -1 if anything
 *   went wrong, 0 if successful.
 */
PRIVATE_EXTERN int
create_path(const char * dirname, mode_t mode)
{
    boolean_t		done = FALSE;
    const char *	scan;

    if (mkdir(dirname, mode) == 0 || errno == EEXIST)
	return (0);

    if (errno != ENOENT)
	return (-1);

    {
	char	path[PATH_MAX];

	for (path[0] = '\0', scan = dirname; done == FALSE;) {
	    const char * 	next_sep;
	    
	    if (scan == NULL || *scan != '/')
		return (FALSE);
	    scan++;
	    next_sep = strchr(scan, '/');
	    if (next_sep == 0) {
		done = TRUE;
		next_sep = dirname + strlen(dirname);
	    }
	    strncpy(path, dirname , next_sep - dirname);
	    path[next_sep - dirname] = '\0';
	    if (mkdir(path, mode) == 0 || errno == EEXIST)
		;
	    else
		return (-1);
	    scan = next_sep;
	}
    }
    return (0);
}

PRIVATE_EXTERN int
ether_cmp(struct ether_addr * e1, struct ether_addr * e2)
{
    int i;
    uint8_t * c1 = e1->octet;
    uint8_t * c2 = e2->octet;

    for (i = 0; i < sizeof(e1->octet); i++, c1++, c2++) {
	if (*c1 == *c2)
	    continue;
	return ((int)*c1 - (int)*c2);
    }
    return (0);
}

PRIVATE_EXTERN void
link_addr_to_string(char * string_buffer, int string_buffer_length,
		    const uint8_t * hwaddr, int hwaddr_len)
{
    int		i;
	    
    switch (hwaddr_len) {
    case 6:
	snprintf(string_buffer, string_buffer_length,
		 EA_FORMAT, EA_LIST(hwaddr));
	break;
    case 8:
	snprintf(string_buffer, string_buffer_length,
		 FWA_FORMAT, FWA_LIST(hwaddr));
	break;
    default: 
	for (i = 0; i < hwaddr_len; i++) {
	    if (i == 0) {
		snprintf(string_buffer, string_buffer_length,
			 "%02x", hwaddr[i]);
		string_buffer += 2;
		string_buffer_length -= 2;
	    }
	    else {
		snprintf(string_buffer, string_buffer_length,
			 ":%02x", hwaddr[i]);
		string_buffer += 3;
		string_buffer_length -= 3;
	    }
	}
	break;
    }
    return;
}

PRIVATE_EXTERN void
fill_with_random(void * buf, uint32_t len)
{
    int			i;
    int			n;
    uint32_t * 		p = (uint32_t *)buf;
    
    n = len / sizeof(*p);
    for (i = 0; i < n; i++, p++) {
	*p = arc4random();
    }
    return;
}

#define ROUNDUP(a) \
    ((a) > 0 ? (1 + (((a) - 1) | (sizeof(u_int32_t) - 1))) : sizeof(u_int32_t))

PRIVATE_EXTERN int
rt_xaddrs(const char * cp, const char * cplim, struct rt_addrinfo * rtinfo)
{
    int 		i;
    struct sockaddr *	sa;

    bzero(rtinfo->rti_info, sizeof(rtinfo->rti_info));
    for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
	if ((rtinfo->rti_addrs & (1 << i)) == 0) {
	    continue;
	}
	sa = (struct sockaddr *)cp;
	if ((cp + sa->sa_len) > cplim) {
	    return (EINVAL);
	}
	rtinfo->rti_info[i] = sa;
	cp += ROUNDUP(sa->sa_len);
    }
    return (0);
}

