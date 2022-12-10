#!/bin/ksh -p

# $$ stores the pid of the running process, it will be unique over time.
builddir="/tmp/tst.$$.tmp"

if ! mkdir $builddir ;
then
	print -u2 "Unable to create the temporary directory ${builddir}";
	exit 1;
fi

cd $builddir

cat > main.c <<EOF
int
main(void) {
	return 0;
}
EOF

if ! xcrun clang -o scéance♥ main.c ;
then
	print -u2 "clang failed ($builddir)";
	exit 1;
fi

if ! dtrace -q -c ./scéance♥ -n 'pid$target::main:entry { printf("%s", probemod); }' ;
then
	print -u2 "dtrace failed ($builddir)";
	exit 1;
fi

cd
rm -r $builddir
