/*
 * ASSERTION:
 *	Verify that pid return probes do trigger more than once
 */
pid$1::f:return
{
	self->i++;
}

pid$1::f:return
/ self->i == 10 /
{
	exit(0);
}

