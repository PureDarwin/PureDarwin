#!/bin/sh -p

############################################################################
# ASSERTION:
#	To verify that dtruss works in its 3 modes of operation


# Check that dtruss "command" works
dtruss /usr/bin/true 1> /dev/null 2> /dev/null
status=$?
if [ $status -ne 0 ]; then
	exit 1
fi

# Check that dtruss -p "pid" works
dtruss -p 1 1> /dev/null 2> /dev/null &

SUBPID=$!

sleep 2

killall -TERM dtrace
wait $SUBPID
status=$?
if [ $status -ne 0 ]; then
	exit 2
fi

# Check that dtruss -W 'processname' works

(
	while [ 1 -eq 1 ]; do
		/usr/bin/true
		sleep 0.1
	done
) &

LOOPPID=$!

dtruss -W true 1> /dev/null 2> /dev/null
status=$?
kill -TERM $LOOPPID
if [ $status -ne 0 ]; then
	exit 3
fi

exit 0
