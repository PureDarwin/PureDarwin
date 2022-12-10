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

#ident	"@(#)tst.D_MACRO_UNUSED.overflow.ksh	1.1	06/08/28 SMI"

############################################################################
# ASSERTION:
# Attempt to pass some arguments and try not to print it.
#
# SECTION: Scripting
#
############################################################################

if [ -f /usr/lib/dtrace/darwin.d ]; then
bname=`/usr/bin/basename $0`
RM=/bin/rm
else
bname=`/bin/basename $0`
RM=/usr/bin/rm
fi
dfilename=/var/tmp/$bname.$$.d

## Create .d file
##########################################################################
cat > $dfilename <<-EOF
#!/usr/sbin/dtrace -qs

BEGIN
{
	exit(0);
}
EOF
##########################################################################


#Call dtrace -C -s <.d>

dtrace -x errtags -s $dfilename "this is test" 1>/dev/null 2>/var/tmp/err.$$.txt
if [ $? -ne 1 ]; then
	print -u2 "Error in executing $dfilename"
	exit 1
fi

grep "D_MACRO_UNUSED" /var/tmp/err.$$.txt >/dev/null 2>&1
if [ $? -ne 0 ]; then
	print -u2 "Expected error D_MACRO_UNUSED not returned"
	$RM -f /var/tmp/err.$$.txt
	exit 1
fi

$RM -f $dfilename
$RM -f /var/tmp/err.$$.txt

exit 0
