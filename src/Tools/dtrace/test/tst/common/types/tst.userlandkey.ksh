#!/bin/ksh

dtrace=$1
exec="tst.userlandkey.exe"

./$exec &
pid=$!

/usr/sbin/dtrace -qs /dev/stdin <<EOF
typedef struct node {
	struct node *next;
	int value;
} node_t;

pid$pid:a.out:list_manipulate:entry
{
	l = (userland node_t*) arg0;

	/* Print value and iterate */
	printf("%d\n", l->value);	l = l->next;
	printf("%d\n", l->value);	l = l->next;
	printf("%d\n", l->value);	l = l->next;
	printf("%d\n", l->value);	l = l->next;
	printf("%d\n", l->value);	l = l->next;
	printf("%d\n", l->value);	l = l->next;
	printf("%d\n", l->value);	l = l->next;
	printf("%d\n", l->value);	l = l->next;

	exit(0);
}
EOF

rc=$?
kill -9 $pid
exit $rc
