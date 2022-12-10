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
dtrace=/usr/sbin/dtrace

pids=$()

# for lib in `ls -1 /lib/lib*.so.1 | grep -v ld.so.1`; do
#	preload=$lib:${preload}
# done

export LD_PRELOAD=$preload

let numkids=100
let i=0

tmpfile=/tmp/dtest.$$

while [ "$i" -lt "$numkids" ]; do
#	sleep 500 &
	sleep 30 &
	pids[$i]=$!
	let i=i+1
done

export LD_PRELOAD=

let i=0

# echo "tick-1sec\n{\n\texit(0);\n}\n" > $tmpfile
echo "tick-1sec{exit(0);}" > $tmpfile

while [ "$i" -lt "$numkids" ]; do
#	echo "pid${pids[$i]}::malloc:entry\n{}\n" >> $tmpfile
	echo "pid${pids[$i]}::malloc:entry{}" >> $tmpfile
	let i=i+1
done

$dtrace -s $tmpfile
status=$?

rm $tmpfile
# pkill sleep
exit $status
