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

#ident	"@(#)tst.AddSearchPath.d.ksh	1.1	06/08/28 SMI"

##
#
# ASSERTION:
# The -I option can be used to search path for #include files when used
# in conjunction with the -C option. The specified directory is inserted into
# the search path adhead of the default directory list.
#
# SECTION: dtrace Utility/-C Option
# SECTION: dtrace Utility/-I Option
#
##

script()
{
	$dtrace -C -I /tmp -s /dev/stdin <<EOF
	#pragma D option quiet
#include "test.h"

	BEGIN
	/1520 != VALUE/
	{
		printf("VALUE: %d, Test should fail\n", VALUE);
		exit(1);
	}

	BEGIN
	/1520 == VALUE/
	{
		printf("VALUE: %d, Test should pass\n", VALUE);
		exit(0);
	}
EOF
}

tempfile=/tmp/test.h
echo "#define VALUE 1520" > $tempfile

dtrace=/usr/sbin/dtrace
script
status=$?

if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
	exit $status
fi

if [ -f /usr/lib/dtrace/darwin.d ]; then
/bin/rm -f $tempfile
else
/usr/bin/rm -f $tempfile
fi
exit 0
