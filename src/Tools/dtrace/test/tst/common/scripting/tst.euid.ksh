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

#ident	"@(#)tst.euid.ksh	1.1	06/08/28 SMI"

############################################################################
# ASSERTION:
#	To verify euid of current process
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
/\$euid != \$1/
{
	exit(1);
}

BEGIN
/\$euid == \$1/
{
	exit(0);
}
EOF
##########################################################################


#Call dtrace -C -s <.d>

chmod 555 $dfilename

userid=`ps -Ao pid,uid | grep "$$ " | awk '{print $2}' 2>/dev/null`
if [ $? -ne 0 ]; then
	echo "unable to get uid of the current process with pid = $$"
	exit 1
fi

$dfilename $userid >/dev/null 2>&1
if [ $? -ne 0 ]; then
	echo "Error in executing $dfilename"
	exit 1
fi

#/usr/bin/rm -f $dfilename
exit 0
