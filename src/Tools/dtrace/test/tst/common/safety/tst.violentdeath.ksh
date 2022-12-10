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
script()
{
	$dtrace -x bufpolicy=ring -x bufsize=1k -s /dev/stdin <<EOF
	fbt:::
	{}
EOF
}

dtrace=/usr/sbin/dtrace

let i=0

while [ "$i" -lt 10 ]; do
	script &
	child=$!
	sleep 1
	if [ -f /usr/lib/dtrace/darwin.d ] ; then
                dtracepid=`ps -ef | grep '[/    ]dtrace'|grep " $child " | awk '{ print $2 }'`
		if [ -n "$dtracepid" ]; then
		    kill -9 $dtracepid 
		else
		    killall dtrace
		fi
		sleep 1
	else
			kill -9 $child
	fi
	let i=i+1
done
