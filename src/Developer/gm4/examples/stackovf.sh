#!/bin/sh

# Script to verify that stack overflow is diagnosed properly when
# there is infinite macro call nesting.
# (causes coredump in m4-1.0.3)

# On some systems the ulimit command is available in ksh or bash but not sh
(exec 2>/dev/null; ulimit -HSs 300) || {
    for altshell in bash bsh ksh ; do
	if (exec >/dev/null 2>&1; $altshell -c 'ulimit -HSs 300') && 
								test -z "$1" 
	then
		echo "Using $altshell because it supports ulimit"
		exec $altshell $0 running-with-$altshell
		exit 9
	fi
    done
}

PATH=.:..:$PATH; export PATH;
M4=m4
type $M4

tmpfile=`tempfile 2> /dev/null` || tmpfile=/tmp/t.$$
trap 'rm -f $tmpfile; exit 1' 1 2 3 15

rm -f core
perl -e '
# Generate nested define sequence
$max=1000000;
for ($i=0; $i<$max; $i++) {
	print "define(X$i,\n";
}
for ($i=$max-1; $i>=0; $i--) {
	print "body with substance no. $i)dnl\n"
}
' | \
(
# Limit the stack size if the shell we are running permits it
if (exec 2>/dev/null; ulimit -HSs 50)
then
	(exec >/dev/null 2>&1; ulimit -v) && ulimitdashv=ok
	ulimit -HSs 50
	#ulimit -HSd 8000
	#test -n "$ulimitdashv" && ulimit -HSv 8000
	echo "Stack limit is `ulimit -s`K";
	echo "Heap limit  is `ulimit -d`K";
	test -n "$ulimitdashv" && 
		echo "VMem limit  is `ulimit -v`K";
else
	echo "Can't reset stack limit - this may take a while..."
fi
$M4 -L999999999 > $tmpfile 2>&1
)
result=$?

exitcode=1
if test $result -eq 0 ; then
    echo "TEST DID NOT WORK - m4 did not abort.  Output:"
else
    # See if stack overflow was diagnosed
    case "`cat $tmpfile`" in
    *overflow*)
	echo "Test succeeded."; 
	exitcode=0
	;;
    *ut*of*emory*)
        echo "*** Test is INCONCLUSIVE (ran out of heap before stack overflow)";
	;;
    *)	echo "*** Test FAILED.  $M4 aborted unexpectedly.  Output:";
	;;
    esac
fi

if test -f core ; then
    ls -l core
    exitcode=1
fi

#(test $exitcode -ne 0) &&
    { echo "Output from $M4:"; cat $tmpfile; }

exit $exitcode
