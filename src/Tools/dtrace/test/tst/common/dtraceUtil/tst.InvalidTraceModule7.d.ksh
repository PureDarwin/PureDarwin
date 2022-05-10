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

#ident	"@(#)tst.InvalidTraceModule7.d.ksh	1.1	06/08/28 SMI"

##
#
# ASSERTION:
# The -m option can be used to enable the probes from their module names.
# Invalid module names result in error.
#
# SECTION: dtrace Utility/-m Option
#
##

dtrace=/usr/sbin/dtrace

if [ -f /usr/lib/dtrace/darwin.d ]; then
$dtrace -m mach_kernel::'{printf("FOUND");}'
else
$dtrace -m genunix::'{printf("FOUND");}'
fi

status=$?

echo $status

if [ "$status" -ne 0 ]; then
	exit 0
fi

echo $tst: dtrace failed
exit $status
