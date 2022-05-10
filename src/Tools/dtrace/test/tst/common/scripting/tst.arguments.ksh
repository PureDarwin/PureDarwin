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

#ident	"@(#)tst.arguments.ksh	1.1	06/08/28 SMI"

############################################################################
# ASSERTION:
#	Pass 10 arguments and try to print them.
#
# SECTION: Scripting
#
############################################################################

if [ -f /usr/lib/dtrace/darwin.d ]; then
bname=`/usr/bin/basename $0`
else
bname=`/bin/basename $0`
fi

dfilename=/var/tmp/$bname.$$


## Create .d file
##########################################################################
cat > $dfilename <<-EOF
#!/usr/sbin/dtrace -qs


BEGIN
{
	printf("%d %d %d %d %d %d %d %d %d %d", \$1, \$2, \$3, \$4, \$5, \$6,
		\$7, \$8, \$9, \$10);
	exit(0);
}
EOF
##########################################################################


#Call dtrace -C -s <.d>

chmod 555 $dfilename


output=`$dfilename 1 2 3 4 5 6 7 8 9 10 2>/dev/null`
if [ $? -ne 0 ]; then
	echo "Error in executing $dfilename"
	exit 1
fi

echo "$output" | awk '{ if ($1 == 1 && $2 == 2 && $3 == 3 && $4 == 4 && $5 == 5 && $6 == 6 && $7 == 7 && $8 == 8 && $9 == 9 && $10 == 10) { exit 0 } else { exit 1 } }'
if [ $? -ne 0 ]; then
	echo "Error in output by $dfilename"
	exit 1
fi

if [ -f /usr/lib/dtrace/darwin.d ]; then
/bin/rm -f $dfilename
else
/usr/bin/rm -f $dfilename
fi
exit 0

