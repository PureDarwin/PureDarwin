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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

BEGIN { DIF_SUBR_MAX = 24; }      /* max subroutine value minus 10 Darwin omissions*/

BEGIN { subr++; @[(long)rand()] = sum(1); }
BEGIN { subr++; @[(long)copyin(NULL, 1)] = sum(1); }
BEGIN { subr++; @str[copyinstr(NULL, 1)] = sum(1); }
BEGIN { subr++; @[(long)speculation()] = sum(1); }
BEGIN { subr++; @[(long)progenyof($pid)] = sum(1); }
BEGIN { subr++; @[(long)strlen("fooey")] = sum(1); }
BEGIN { subr++; }
BEGIN { subr++; }
BEGIN { subr++; @[(long)alloca(10)] = sum(1); }
BEGIN { subr++; }
BEGIN { subr++; }
BEGIN { subr++; @[(long)getmajor(0)] = sum(1); }
BEGIN { subr++; @[(long)getminor(0)] = sum(1); }
BEGIN { subr++; @str[strjoin("foo", "bar")] = sum(1); }
BEGIN { subr++; @str[lltostr(12373)] = sum(1); }
BEGIN { subr++; @str[basename("/var/crash/systemtap")] = sum(1); }
BEGIN { subr++; @str[dirname("/var/crash/systemtap")] = sum(1); }
BEGIN { subr++; @str[cleanpath("/var/crash/systemtap")] = sum(1); }
BEGIN { subr++; @str[strchr("The SystemTap, The.", 't')] = sum(1); }
BEGIN { subr++; @str[strrchr("The SystemTap, The.", 't')] = sum(1); }
BEGIN { subr++; @str[strstr("The SystemTap, The.", "The")] = sum(1); }
BEGIN { subr++; @str[strtok("The SystemTap, The.", "T")] = sum(1); }
BEGIN { subr++; @str[substr("The SystemTap, The.", 0)] = sum(1); }
BEGIN { subr++; @[(long)index("The SystemTap, The.", "The")] = sum(1); }
BEGIN { subr++; @[(long)rindex("The SystemTap, The.", "The")] = sum(1); }


BEGIN
/subr == DIF_SUBR_MAX + 1/
{
	exit(0);
}

BEGIN
{
	printf("found %d subroutines, expected %d\n", subr, DIF_SUBR_MAX + 1);
	exit(1);
}
