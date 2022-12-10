#!/bin/sh -p
dtrace=/usr/sbin/dtrace


$dtrace -xnolibs -c ./tst.oneshot_multithreaded.exe -qs /dev/stdin <<EOF
oneshot\$target::f:entry {
	hit = 1;
}

tick-1sec / hit == 1 / {
	exit(42);
}

EOF


status=$?
# If we exited with a zero exit code, it means the target process crashed
if [ $status -eq 42 ]; then
	exit 0
elif [ $status -eq 0 ]; then
	exit 1
else
	exit $status
fi
