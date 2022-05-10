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
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

#ident	"@(#)tst.ZeroProbesWithoutZ.d.ksh	1.1	06/08/28 SMI"

##
#
# ASSERTION:
# Without the -Z option probe descriptions that do not match any known
# probes will cause an error or will not be enabled.
#
# SECTION: dtrace Utility/-Z Option
#
##

dtrace=/usr/sbin/dtrace

$dtrace -qP wassup'{printf("Iamkool");}' \
-qP profile'{printf("I am done"); exit(0);}'

status=$?

if [ "$status" -ne 0 ]; then
	exit 0
fi

echo $tst: dtrace failed
exit $status
