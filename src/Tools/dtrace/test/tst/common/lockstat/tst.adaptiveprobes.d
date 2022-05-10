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
 * 	Check the presence and trigger of lockstat adaptive mutex probes
 */

int probes[string];

BEGIN
/ `MutexSpin > 100000 /
{
	/*
	 * On virtual machines, the adaptive spinning timeout is set to a
	 * high-enough value that block probes may never trigger
	 */
	probes["adaptive-block"] = 2;
}

lockstat:::adaptive-acquire,
lockstat:::adaptive-block,
lockstat:::adaptive-spin,
lockstat:::adaptive-release
{
	probes[probename] = 1
}

tick-500ms
/ probes["adaptive-acquire"] &&
  probes["adaptive-block"] &&
  probes["adaptive-spin"] &&
  probes["adaptive-release"] /
{
	exit(0);
}

tick-60s
{
	printf("adaptive-acquire: %d\n", probes["adaptive-acquire"]);
	printf("adaptive-block: %d\n", probes["adaptive-block"]);
	printf("adaptive-spin: %d\n", probes["adaptive-spin"]);
	printf("adaptive-release: %d\n", probes["adaptive-release"]);
	exit(1);
}
