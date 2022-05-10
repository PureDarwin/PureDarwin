#!/bin/sh -p
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2018 Apple, Inc.  All rights reserved.
# Use is subject to license terms.
#

############################################################################
# ASSERTION:
#	To verify that Objective-C trampolines are properly blacklisted

dtrace -Z -xnolibs -c ./tst.TrampolineBlacklist.exe -qs /dev/stdin <<EOF
pid\$target:libobjc-trampolines.dylib::entry,
pid\$target::_a1a2_firsttramp:entry,
pid\$target::_a1a2_nexttramp:entry,
pid\$target::_a1a2_trampend:entry,
pid\$target::_a1a2_tramphead:entry,
pid\$target::_a1a3_firsttramp:entry,
pid\$target::_a1a3_nexttramp:entry,
pid\$target::_a1a3_trampend:entry,
pid\$target::_a1a3_tramphead:entry
{
	printf("trampoline %s ran\n", probefunc);
	exit(1);
}

proc:::signal-handle
/ pid == \$target /
{
	printf("target process got killed\n");
	exit(2);
}
EOF
exit $?
