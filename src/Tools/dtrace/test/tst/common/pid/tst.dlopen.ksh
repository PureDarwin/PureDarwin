file=out.$$
dtrace=/usr/sbin/dtrace

cmd=`pwd`/tst.dlopen.exe

rm -f $file

$dtrace -o $file -c $cmd -s /dev/stdin <<EOF
	#pragma D option destructive
	#pragma D option zdefs
	
	pid\$target::waiting:entry
	{
		this->value = (int *)alloca(sizeof (int));
		*this->value = 1;
		copyout(this->value, arg0, sizeof (int));
	}

	pid\$target::pcre_config:entry
	{
		exit(0);
	}

	BEGIN
	{
		/*
		 * Let's just do this for 5 seconds.
		 */
		timeout = timestamp + 50000000000;
	}

	profile:::tick-4
	/timestamp > timeout/
	{
		trace("test timed out");
		exit(1);
	}
EOF

status=$?
if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
	exit $status
fi

if [ "$status" -eq 0 ]; then
	rm -f $file
fi

exit $status
