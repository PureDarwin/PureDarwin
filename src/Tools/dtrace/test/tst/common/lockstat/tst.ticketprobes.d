/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2019 Apple, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * ASSERTION:
 * 	Check the presence and trigger of lockstat ticket lock probes
 */

int probes[string];

lockstat:::ticket-spin, lockstat:::ticket-acquire, lockstat:::ticket-release
{
	probes[probename] = 1
}

tick-500ms
/ probes["ticket-spin"] &&
  probes["ticket-acquire"] &&
  probes["ticket-release"] /
{
	exit(0);
}

tick-60s
{
	printf("ticket-spin: %d\n", probes["ticket-spin"]);
	printf("ticket-acquire: %d\n", probes["ticket-acquire"]);
	printf("ticket-release: %d\n", probes["ticket-release"]);
	exit(1);
}
