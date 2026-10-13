/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#ifdef __FBSDID
__FBSDID("$FreeBSD: src/sbin/gpt/show.c,v 1.14 2006/06/22 22:22:32 marcel Exp $");
#endif
#ifdef __RCSID
__RCSID("$NetBSD: show.c,v 1.50 2026/03/28 20:43:34 palle Exp $");
#endif

#include <sys/bootblock.h>
#include <sys/types.h>
#if defined(HAVE_SYS_ENDIAN_H) || ! defined(HAVE_NBTOOL_CONFIG_H)
#include <sys/endian.h>
#endif

#include <err.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#ifndef HAVE_NBTOOL_CONFIG_H
#include <sys/ioctl.h>
#endif

#include "map.h"
#include "gpt.h"
#include "gpt_private.h"

static int cmd_show(gpt_t, int, char *[]);

static const char *showhelp[] = {
	"[-AagHhlux] [-b start_block] [-i index] [ -W width ]",
};

#define SHOW_UUID	0x0001
#define SHOW_GUID	0x0002
#define SHOW_LABEL	0x0004
#define SHOW_ALL	0x0008
#define SHOW_HEX	0x0010
#define SHOW_HUMAN	0x0020
#define SHOW_DECIMAL	0x0040
#define SHOW_APPROX	0x0080
#define SHOW_PARSABLE	0x0100

#define SHOW_NOSHOW	0x10000

const struct gpt_cmd c_show = {
	"show",
	cmd_show,
	showhelp, __arraycount(showhelp),
	GPT_READONLY,
};

#define usage() gpt_usage(NULL, &c_show)

static unsigned int out_width;

static const char *
get_mbr_sig(char *b, size_t blen, const uint8_t *bp)
{
	gpt_uuid_t uuid;

	/*
	 * MBR partitions have a 4 byte signature in the MBR.  Table
	 * 10.54 of UEFI Spec 2.10 Errata A states how this is to be
	 * formatted as a GUID.
	 *
	 * XXX: I thought I had seen more on this elsewhere, but I
	 * can't seem to find it now.  In particular, the endianness
	 * of this quanity is not clear in the above.
	 *
	 * XXX: The location and size of the MBR signature should be
	 * in 'struct mbr,' e.g.:
	 *
	 * struct mbr {
	 *	uint8_t		mbr_code[440];
	 *	uint32_t	mbr_disc_sig;
	 *	uint16_t	mbr_unknown;
	 *	struct mbr_part	mbr_part[4];
	 *	uint16_t	mbr_sig;
	 * };
	 *
	 * For now, we just hardcode it.  Ugh!
	 */
	memset(uuid, 0, sizeof(uuid));
	memcpy(uuid, bp + 440, 4);
	gpt_uuid_snprintf(b, blen, "%d", uuid);
	return b;
}

static const char *
get_gpt_hdr_guid(char *b, size_t blen, struct gpt_hdr *hdr)
{
	gpt_uuid_snprintf(b, blen, "%d", hdr->hdr_guid);
	return b;
}

static char *
parsable_label(gpt_t gpt, struct gpt_ent *ent)
{
	char *res;
	char *ol;
	uint8_t utfbuf[__arraycount(ent->ent_name) * 3 + 1];

	if (ent->ent_name[0] == 0)
		return NULL;

	utf16_to_utf8(ent->ent_name,
	    __arraycount(ent->ent_name), utfbuf,
	    __arraycount(utfbuf));

	ol = setlocale(LC_CTYPE, "en_GB.UTF-8");
	stravis(&res, (char *)utfbuf, VIS_CSTYLE|VIS_OCTAL|VIS_TAB|VIS_NL);
	if (ol)
		setlocale(LC_CTYPE, ol);

	return res;
}

static int
print_part_type(gpt_t gpt, int map_type, int flags, void *map_data,
    off_t map_start)
{
	off_t start;
	map_t p;
	struct mbr *mbr;
	struct gpt_ent *ent;
	unsigned int i;
	char buf[128], *b = buf;
	uint8_t utfbuf[__arraycount(ent->ent_name) * 3 + 1];
	int len = 0;

	switch (map_type) {
	case MAP_TYPE_UNUSED:
		len += printf("Unused");
		break;
	case MAP_TYPE_MBR:
		if (map_start != 0)
			len += printf("Extended ");
		len += printf("MBR");
		if (map_start == 0 && flags & SHOW_GUID)
			len += printf(" - %s",
			    get_mbr_sig(buf, sizeof(buf), map_data));
		break;
	case MAP_TYPE_PRI_GPT_HDR:
		len += printf("Pri GPT header");
		if (flags & SHOW_GUID)
			len += printf(" - %s",
			    get_gpt_hdr_guid(buf, sizeof(buf), map_data));
		break;
	case MAP_TYPE_SEC_GPT_HDR:
		len += printf("Sec GPT header");
		if (flags & SHOW_GUID)
			len += printf(" - %s",
			    get_gpt_hdr_guid(buf, sizeof(buf), map_data));
		break;
	case MAP_TYPE_PRI_GPT_TBL:
		len += printf("Pri GPT table");
		break;
	case MAP_TYPE_SEC_GPT_TBL:
		len += printf("Sec GPT table");
		break;
	case MAP_TYPE_MBR_PART:
		p = map_data;
		if (p->map_start != 0)
			len += printf("Extended ");
		len += printf("MBR part ");
		mbr = p->map_data;
		for (i = 0; i < 4; i++) {
			start = le16toh(mbr->mbr_part[i].part_start_hi);
			start = (start << 16) +
			    le16toh(mbr->mbr_part[i].part_start_lo);
			if (map_start == p->map_start + start)
				break;
		}
		if (i == 4) {
			/* wasn't there */
			len += printf("[partition not found?]");
		} else {
			len += printf("%d%s", mbr->mbr_part[i].part_typ,
			    mbr->mbr_part[i].part_flag == 0x80 ?
			    " (active)" : "");
		}
		break;
	case MAP_TYPE_GPT_PART:
		len += printf("GPT part ");
		ent = map_data;
		if (flags & SHOW_LABEL && ent->ent_name[0] != 0) {
			if (flags & SHOW_PARSABLE)
				b = parsable_label(gpt, ent);
			else {
				utf16_to_utf8(ent->ent_name,
				    __arraycount(ent->ent_name), utfbuf,
				    __arraycount(utfbuf));
				b = (char *)utfbuf;
			}
		} else if (flags & SHOW_GUID) {
			gpt_uuid_snprintf(buf, sizeof(buf), "%d",
			    ent->ent_guid);
		} else if (flags & SHOW_UUID) {
			gpt_uuid_snprintf(buf, sizeof(buf), "%d",
			    ent->ent_type);
		} else {
			gpt_uuid_snprintf(buf, sizeof(buf), "%l",
			    ent->ent_type);
		}
		if (b != NULL)
			len += printf("- %s", b);
		if (b != buf && b != (char *)utfbuf)
			free(b);
		break;
	case MAP_TYPE_PMBR:
		len += printf("PMBR");
		mbr = map_data;
		if (mbr->mbr_part[0].part_typ == MBR_PTYPE_PMBR &&
		    mbr->mbr_part[0].part_flag == 0x80)
			    len += printf(" (active)");
		break;
	default:
		len += printf("Unknown %#x", map_type);
		break;
	}
	return len;
}

static char *
cvt_size(gpt_t gpt, uintmax_t sz, int xshow, char *b, unsigned int bs)
{
	char *p;
	const char *u;
	uintmax_t num;
	int plus = 0;

#define SFMT (xshow & SHOW_HEX) ?    "%jx" :    "%ju"
#define FMT  (xshow & SHOW_HEX) ? "  %*jx" : "  %*ju"

	if (bs >= INT_MAX)
		bs = INT_MAX / 2;

	num = sz * gpt->secsz;

	if (num == 0 || (xshow & SHOW_HUMAN) == 0) {
 bale:
		if (snprintf(b, bs, SFMT, sz) <= (int)bs)
			return b;
		if (!(xshow & SHOW_NOSHOW))
			printf(FMT, gpt->lbawidth, sz);
		return NULL;
	}

	sz = num;	/* so if we bale after this we get bytes, not sectors */

#ifdef HN_AUTOSCALE
	if (xshow & SHOW_APPROX) {
		int flags = HN_NOSPACE|HN_B|HN_DECIMAL;

		if (xshow & SHOW_DECIMAL)
			flags |= HN_DIVISOR_1000;

		if (humanize_number(b, (bs > 6 ? 5 : bs), (int64_t)num,
		    "", HN_AUTOSCALE, flags) < 0)
			goto bale;
		return b;
	}
#endif

	u = "BKMGTPE";
	p = b + bs - 1;
	*p = '\0';

		ssize_t n = 0;
	while (num != 0 && *u != '\0') {
		char unitbuf[32];
		uintmax_t val;

		if (xshow & SHOW_DECIMAL)
			val = num % 1000;
		else
			val = num & 0x3FF;

		if (u[1] == '\0' || val != 0) {
			n = snprintf(unitbuf, sizeof unitbuf, SFMT,
				u[1] == '\0' ? num : val);
			if (n < 0)
				goto bale;
			if (b + n + plus >= p)
				break;
			p -= n + 1 + plus;
			(void)strcpy(p, unitbuf);
			p[n++] = *u;
			if (plus)
				p[n] = ' '; /* or '_' or '+' */
			plus = 1;
		}

		if (xshow & SHOW_DECIMAL)
			num /= 1000;
		else
			num >>= 10;
		u++;
	}
	if (*u != '\0' && num != 0)
		goto bale;

	return p;
}

static int
show(gpt_t gpt, int xshow)
{
	map_t m;
	char *p;
	unsigned int w1 = (unsigned int)gpt->lbawidth + 2;
	unsigned int w2;
	char szbuf[64];

	if ((size_t)w1 > sizeof szbuf - 2)
		w1 = sizeof szbuf - 2;
	w2 = w1;

	if (xshow & SHOW_HUMAN) {
		if (xshow & SHOW_APPROX) {
			if (w1 > 8)
				w2 = w1 = 7;
		} else {
			w1 = w2 = 0;
			m = map_first(gpt);
			while (m != NULL) {
				size_t l;

				p = cvt_size(gpt, (uintmax_t)m->map_start,
				    xshow | SHOW_NOSHOW, szbuf, sizeof szbuf);
				if (p != NULL && (l = strlen(p)) > (size_t)w1)
					w1 = (unsigned int)l;
				p = cvt_size(gpt, (uintmax_t)m->map_size,
				    xshow | SHOW_NOSHOW, szbuf, sizeof szbuf);
				if (p != NULL && (l = strlen(p)) > (size_t) w2)
					w2 = (unsigned int)l;

				m = m->map_next;
			}
		}
	}
	w2 += 2;	/* space between columns */

	if ((size_t)w1 >= sizeof szbuf)
		w1 = (unsigned int)sizeof szbuf - 1;
	if ((size_t)w2 >= sizeof szbuf)
		w2 = (unsigned int)sizeof szbuf - 1;

	if (!(xshow & SHOW_PARSABLE)) {
		if (w1 < 5 /* strlen("start")) */)
			w1 = 5;
		if (w2 < 4 /* strlen("size")) */)
			w2 = 4;
		printf("%*s", w1, "start");
		printf("%*s", w2, "size");
		printf("  index  contents\n");
	}

	m = map_first(gpt);
	if (xshow & SHOW_PARSABLE) {
		while (m != NULL) {
			printf("%ju %ju %u ", (uintmax_t)m->map_start,
			    (uintmax_t)m->map_size, m->map_index);
			print_part_type(gpt, m->map_type, xshow, m->map_data,
			    m->map_start);
			putchar('\n');
			m = m->map_next;
		}
		return 0;
	}

	while (m != NULL) {
		p = cvt_size(gpt, (uintmax_t)m->map_start, xshow, szbuf, w1+1);
		if (p != NULL)
			printf("%*s", w1, p);

		p = cvt_size(gpt, (uintmax_t)m->map_size, xshow, szbuf, w2+1);
		if (p != NULL)
			printf("%*s", w2, p);

		putchar(' ');
		putchar(' ');
		if (m->map_index > 0)
			printf("%5d", m->map_index);
		else
			printf("     ");
		putchar(' ');
		putchar(' ');
		print_part_type(gpt, m->map_type, xshow, m->map_data,
		    m->map_start);
		putchar('\n');
		m = m->map_next;
	}
	return 0;
}

static void
gpt_show_sec_num(const char *label, int64_t secsize, off_t num, int xshow,
    gpt_t gpt)
{
	char *p = NULL;
	char size_string[64];
#ifdef HN_AUTOSCALE
	char human_num[5];
#endif

	if (xshow & SHOW_HUMAN
#ifdef HN_AUTOSCALE
	    && !(xshow & SHOW_APPROX)
#endif
					) {
		p = cvt_size(gpt, (uintmax_t)num, xshow,
		    size_string, sizeof size_string);
	}
#ifdef HN_AUTOSCALE
	if (p == NULL && humanize_number(p = human_num, sizeof(human_num),
	    (int64_t)num*secsize,
	    "", HN_AUTOSCALE, HN_NOSPACE|HN_B) < 0)
		human_num[0] = '\0';
#endif
	printf("%s: %" PRIu64, label, (uint64_t)num);
	if (p != NULL && p[0] != '\0')
		printf(" (%s)", p);
	printf("\n");
}


static int
show_parsable(gpt_t gpt, map_t m, struct gpt_ent *ent, int xshow)
{
	char s1[128], s2[128], *p;

	printf("Index: %u\n", m->map_index);
	printf("Start: %ju\n", (uintmax_t)m->map_start);
	printf("Size: %ju\n", (uintmax_t)m->map_size);

	if (gpt->verbose)
		printf("Map_Type: %d\n", m->map_type);

	if (m->map_type != MAP_TYPE_GPT_PART) {
		printf("Purpose: ");
		print_part_type(gpt, m->map_type, 0, m->map_data,
		    m->map_start);
		putchar('\n');
		if (m->map_type == MAP_TYPE_PRI_GPT_HDR ||
		    m->map_type == MAP_TYPE_SEC_GPT_HDR) {
			get_gpt_hdr_guid(s1, sizeof(s1), m->map_data);
			printf("GUID: %s\n", s1);
		}
		return 0;
	}

	gpt_uuid_snprintf(s2, sizeof(s2), "%d", ent->ent_guid);
	printf("GUID: %s\n", s2);

	gpt_uuid_snprintf(s1, sizeof(s1), "%s", ent->ent_type);
	gpt_uuid_snprintf(s2, sizeof(s2), "%d", ent->ent_type);
	printf("TypeID: %s\n", s2);
	if (strcmp(s1, s2) != 0)
		printf("Type: %s\n", s1);
	gpt_uuid_snprintf(s1, sizeof(s1), "%l", ent->ent_type);
	if (strcmp(s1, s2) != 0)
		printf("Long_Type: %s\n", s1);


	if (ent->ent_attr != 0) {
		char buf[1024];

		printf("Attributes: %s\n",
		    gpt_attr_list(buf, sizeof(buf), ent->ent_attr));
	}

	p = parsable_label(gpt, ent);
	if (p != NULL) {
		printf("Label: %s\n", p );
		free(p);
	}

	return 0;
}

static int
show_one(gpt_t gpt, unsigned int entry, int xshow)
{
	map_t m;
	struct gpt_ent *ent;
	char s1[128], s2[128];
	uint8_t utfbuf[__arraycount(ent->ent_name) * 3 + 1];

	for (m = map_first(gpt); m != NULL; m = m->map_next)
		if (entry == m->map_index)
			break;
	if (m == NULL) {
		gpt_warnx(gpt, "Could not find index %d", entry);
		return -1;
	}
	ent = m->map_data;

	if (xshow & SHOW_PARSABLE)
		return show_parsable(gpt, m, ent, xshow);

	printf("Details for index %d:\n", entry);
	gpt_show_sec_num("Start", gpt->secsz, m->map_start, xshow, gpt);
	gpt_show_sec_num("Size", gpt->secsz, m->map_size, xshow, gpt);

	gpt_uuid_snprintf(s1, sizeof(s1), "%s", ent->ent_type);
	gpt_uuid_snprintf(s2, sizeof(s2), "%d", ent->ent_type);
	if (strcmp(s1, s2) == 0)
		strlcpy(s1, "unknown", sizeof(s1));
	printf("Type: %s (%s)\n", s1, s2);

	gpt_uuid_snprintf(s2, sizeof(s2), "%d", ent->ent_guid);
	printf("GUID: %s\n", s2);

	utf16_to_utf8(ent->ent_name, __arraycount(ent->ent_name), utfbuf,
	    __arraycount(utfbuf));
	printf("Label: %s\n", (char *)utfbuf);

	printf("Attributes: ");
	if (ent->ent_attr == 0) {
		printf("None\n");
	} else  {	
		char buf[1024];
		printf("%s\n", gpt_attr_list(buf, sizeof(buf), ent->ent_attr));
	}

	return 0;
}

static int
show_all(gpt_t gpt, int xshow)
{
	map_t m;
	struct gpt_ent *ent;
	char s1[128], s2[128];
	char human_num[8];
	uint8_t utfbuf[__arraycount(ent->ent_name) * 3 + 1];
	unsigned int width;
	char *p;

	m = map_first(gpt);

	if (xshow & SHOW_PARSABLE) {
		while (m != NULL) {
			ent = m->map_data;
			show_parsable(gpt, m, ent, xshow);
			putchar('\n');
			m = m->map_next;
		}
		return 0;
	}

	printf("  %*s", gpt->lbawidth, "start");
	printf("  %*s", gpt->lbawidth, "size");
	printf("  index  contents\n");

	width = 2 * ((unsigned)gpt->lbawidth + 2) + 7;

	while (m != NULL) {
		printf(FMT, gpt->lbawidth, (uintmax_t)m->map_start);
		printf(FMT, gpt->lbawidth, (uintmax_t)m->map_size);
		putchar(' ');
		putchar(' ');
		if (m->map_index > 0) {
			printf("%5d  ", m->map_index);
			print_part_type(gpt, m->map_type, 0, m->map_data,
			    m->map_start);
			putchar('\n');

			ent = m->map_data;

			gpt_uuid_snprintf(s1, sizeof(s1), "%s", ent->ent_type);
			gpt_uuid_snprintf(s2, sizeof(s2), "%d", ent->ent_type);
			if (strcmp(s1, s2) == 0)
				strlcpy(s1, "unknown", sizeof(s1));
			printf("%*s  Type: %s\n", width, "", s1);
			if (m->map_type == MAP_TYPE_MBR_PART) {
				static uint8_t unused_uuid[sizeof(gpt_uuid_t)];
				/*
				 * MBR part partitions don't have
				 * GUIDs, so don't create a bogus one!
				 *
				 * We could get the TypeID from the
				 * partition type (the one byte OSType
				 * field in the partition structure),
				 * perhaps borrowing info from fdisk.
				 * However, some OSTypes have multiple
				 * OSes assigned to them and many may
				 * not have official UUIDs.
				 *
				 * Should we even print anything for
				 * these, in particular the GUID?
				 */
				gpt_uuid_snprintf(s2, sizeof(s2), "%d",
				    unused_uuid);

				printf("%*s  TypeID: %s\n", width, "", s2);
				printf("%*s  GUID: %s\n", width, "", s2);
			} else {
				printf("%*s  TypeID: %s\n", width, "", s2);
				gpt_uuid_snprintf(s2, sizeof(s2), "%d",
				    ent->ent_guid);
				printf("%*s  GUID: %s\n", width, "", s2);
			}

			printf("%*s  Size: ", width, "");
			human_num[0] = '\0';
#ifdef HN_AUTOSCALE
			if (humanize_number(human_num, sizeof(human_num),
			    (int64_t)(m->map_size * gpt->secsz),
			    "", HN_AUTOSCALE, (xshow & SHOW_DECIMAL) ?
			    HN_DIVISOR_1000|HN_B : HN_B) >= 0)
				printf("%s ", human_num);
#endif
			printf("%s%ju%s", human_num[0] ? "(" : "",
			    (intmax_t)m->map_size * gpt->secsz,
			    human_num[0] ? ")" : "");

			if (xshow & SHOW_HUMAN) {
				p = cvt_size(gpt, (uintmax_t)m->map_size, xshow,
				    s1, sizeof s1);
				if (p)
					printf(" = %s", p);
			}
			putchar('\n');

			utf16_to_utf8(ent->ent_name,
			    __arraycount(ent->ent_name), utfbuf,
			    __arraycount(utfbuf));
			printf("%*s  Label: %s\n", width, "", (char *)utfbuf);

			printf("%*s  Attributes: ", width, "");
			if (ent->ent_attr == 0) {
				printf("None\n");
			} else  {	
				char buf[1024];

				printf("%s\n", gpt_attr_list(buf, sizeof(buf),
				    ent->ent_attr));
			}
		} else {
			unsigned int pr = (unsigned)printf("       ");
			pr += (unsigned)print_part_type(gpt, m->map_type, 0,
			    m->map_data, m->map_start);
			if (xshow & SHOW_HUMAN) {
				int bw = 4;

				p = cvt_size(gpt, (uintmax_t)m->map_size,
					xshow | SHOW_NOSHOW, s1, sizeof s1);

				if (p != NULL) {
					unsigned int w = (unsigned)strlen(p);

					if (w < 4)
						w = 4;

					if (width + pr + w + 4 < out_width) {
						w = out_width - 1 -
						    width - pr - w;
					} else
						bw = 1, w = 2;

					printf("%*s[ %*s ]", w, "", bw, p);
				}
			}
			putchar('\n');

			switch (m->map_type) {
			case MAP_TYPE_PRI_GPT_HDR:
			case MAP_TYPE_SEC_GPT_HDR:
				printf("%*s  GUID: %s\n", width, "",
				    get_gpt_hdr_guid(s1, sizeof(s1), 
				    m->map_data));
				break;
			case MAP_TYPE_MBR:
				printf("%*s  GUID: %s\n", width, "",
				    get_mbr_sig(s1, sizeof(s1), m->map_data));
				break;
			default:
				break;
			}
		}
		m = m->map_next;
	}
	return 0;
}

static unsigned int
get_terminal_width(void)
{
	unsigned int w;
	char *p;
	int err;
#ifndef HAVE_NBTOOL_CONFIG_H
	struct winsize wsz;
#endif

	if ((p = getenv("COLUMNS")) != NULL) {
		w = (unsigned int)strtou(p, NULL, 10, 0, 3000, &err);
		if (err == 0)
			return w;
	}
#ifndef HAVE_NBTOOL_CONFIG_H
	if (ioctl(fileno(stdout), TIOCGWINSZ, &wsz) == 0)
		return wsz.ws_col;
#endif
	return 0;
}

static int
cmd_show(gpt_t gpt, int argc, char *argv[])
{
	int ch;
	int xshow = 0;
	int wide = 0;
	unsigned int entry = 0;
	off_t start = 0;
	map_t m;

	out_width = 0;

	while ((ch = getopt(argc, argv, "gi:b:luaxAhHpW:w")) != -1) {
		switch(ch) {
		case 'a':
			xshow |= SHOW_ALL;
			break;
		case 'g':
			xshow |= SHOW_GUID;
			break;
		case 'i':
			if (gpt_uint_get(gpt, &entry) == -1)
				return usage();
			break;
		case 'b':
			if (gpt_human_get(gpt, &start) == -1)
				return usage();
			break;
		case 'l':
			xshow |= SHOW_LABEL;
			break;
		case 'u':
			xshow |= SHOW_UUID;
			break;
		case 'x':
			xshow |= SHOW_HEX;
			break;
		case 'A':
			xshow |= SHOW_HUMAN | SHOW_APPROX;
			break;
		case 'h':
			xshow |= SHOW_HUMAN;
			break;
		case 'H':
			xshow |= SHOW_HUMAN | SHOW_DECIMAL;
			break;
		case 'p':
			xshow |= SHOW_PARSABLE;
			break;
		case 'W':
			if (gpt_uint_get(gpt, &out_width) < 0)
				usage();
			break;
		case 'w':
			if (wide < 10)	/* avoid overflow! */
				wide++;
			break;
		default:
			return usage();
		}
	}

	if (out_width == 0) {
		out_width = get_terminal_width();
		if (out_width == 0) {
			switch (wide) {
			case  0: out_width = 80;  break;
			case  1: out_width = 120; break;
			case  2: out_width = 200; break;
			default: out_width = 320; break;
			}
		}
	}

	/* This restriction is important to keep the output syntax constant */
	if (xshow & SHOW_PARSABLE)
		xshow &= ~(SHOW_HUMAN | SHOW_APPROX | SHOW_DECIMAL | SHOW_HEX);

	/* This one is just a preventative measure for human sanity */
	if (xshow & SHOW_HUMAN)
		xshow &= ~SHOW_HEX;

	if (argc != optind)
		return usage();

	if ((map_find(gpt, MAP_TYPE_PRI_GPT_HDR) == NULL) &&
	    ! (gpt->flags & GPT_QUIET) )
		printf("GPT not found, displaying data from MBR.\n\n");

	if (xshow & SHOW_ALL)
		return show_all(gpt, xshow);

	if (start > 0) {
		for (m = map_first(gpt); m != NULL; m = m->map_next) {
			if (m->map_type != MAP_TYPE_GPT_PART ||
			    m->map_index < 1)
				continue;
			if (start != m->map_start)
				continue;
			entry = m->map_index;
			break;
		}
	}

	return entry > 0 ? show_one(gpt, entry, xshow) : show(gpt, xshow);
}
