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
 * Copyright 2018 Apple, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * ASSERTION: Anonymous union/struct fields can be accessed and set inside
 * a union
 *
 * SECTION: Structs and Unions/Structs
 *
 */
#pragma D option quiet

union record {
	int a;
	struct {
		int b;
	};
	union {
	     int c;
	     int d;
	};
} var;

BEGIN
{
	var.a = 1;
}

BEGIN / var.a != 1 / { exit(1) }
BEGIN { var.b = 2; }
BEGIN / var.b != 2 / { exit(2) }
BEGIN { var.c = 3; }
BEGIN / var.c != 3 / { exit(3) }
BEGIN { var.d = 4; }
BEGIN / var.d != 4 / { exit(4) }
BEGIN { exit(0); }
ERROR
{
	exit(5);
}

