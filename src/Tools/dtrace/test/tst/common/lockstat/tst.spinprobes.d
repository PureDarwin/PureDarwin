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
 * 	Check the presence and trigger of lockstat spinlock probes
 */

int probes[string];

lockstat:::spin-spin, lockstat:::spin-acquire, lockstat:::spin-release
{
	probes[probename] = 1
}

tick-500ms
/ probes["spin-spin"] &&
  probes["spin-acquire"] &&
  probes["spin-release"] /
{
	exit(0);
}

tick-60s
{
	printf("spin-spin: %d\n", probes["spin-spin"]);
	printf("spin-acquire: %d\n", probes["spin-acquire"]);
	printf("spin-release: %d\n", probes["spin-release"]);
	exit(1);
}
