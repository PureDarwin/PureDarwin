/*-
 * Copyright (c) 2004 Marcel Moolenaar
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
__FBSDID("$FreeBSD: src/sbin/gpt/remove.c,v 1.10 2006/10/04 18:20:25 marcel Exp $");
#endif
#ifdef __RCSID
__RCSID("$NetBSD: uuid.c,v 1.4 2025/02/23 20:47:19 christos Exp $");
#endif

#include <sys/types.h>

#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "map.h"
#include "gpt.h"
#include "gpt_private.h"

static int cmd_uuid(gpt_t, int, char *[]);

static const char *uuidhelp[] = {
	"-a",
	"[-b blocknr] [-i index] [-L label] [-s sectors] [-t type] [-U newuuid]",
};

const struct gpt_cmd c_uuid = {
	"uuid",
	cmd_uuid,
	uuidhelp, __arraycount(uuidhelp),
	GPT_SYNC,
};

#define usage() gpt_usage(NULL, &c_uuid)

struct uuidctx {
	gpt_t gpt;
	gpt_uuid_t *uuid;
};

static void
change_ent(struct gpt_ent *ent, void *v, int backup)
{
	struct uuidctx *ctx = v;
	static gpt_uuid_t uuidstore;

	if (!backup) {
		if (ctx->uuid != NULL)
			memcpy(uuidstore, ctx->uuid, sizeof(uuidstore));
		else
			gpt_uuid_generate(ctx->gpt, uuidstore);
	}
	memmove(ent->ent_guid, uuidstore, sizeof(ent->ent_guid));
}

static void
change_hdr(struct gpt_hdr *hdr, void *v, int backup)
{
	struct uuidctx *ctx = v;
	static gpt_uuid_t uuidstore;

	if (!backup)
		gpt_uuid_generate(ctx->gpt, uuidstore);
	memmove(hdr->hdr_guid, uuidstore, sizeof(hdr->hdr_guid));
}

static int
cmd_uuid(gpt_t gpt, int argc, char *argv[])
{
	int ch, rc;
	struct gpt_find find;
	gpt_uuid_t new_uuid;
	struct uuidctx ctx;

	if (gpt == NULL)
		return usage();

	memset(&find, 0, sizeof(find));
	find.msg = "UUID changed";

	/* Get the uuid options */
	ctx.gpt = gpt;
	ctx.uuid = NULL;
	while ((ch = getopt(argc, argv, GPT_FIND "U:")) != -1) {
		switch (ch) {
		case 'U':
			if (gpt_uuid_parse(optarg, new_uuid) == -1)
				return usage();
			ctx.uuid = &new_uuid;
			break;
		case 'L':
		case 'a':
		case 'b':
		case 'i':
		case 's':
		case 't':
			if (gpt_add_find(gpt, &find, ch) == -1)
				return usage();
			break;
		default:
			return usage();
		}
	}

	if (argc != optind)
		return usage();

	rc = gpt_change_ent(gpt, &find, change_ent, &ctx);
	if (rc != 0)
		return rc;

	if (find.all)
		return gpt_change_hdr(gpt, &find, change_hdr, &ctx);

	return 0;
}
