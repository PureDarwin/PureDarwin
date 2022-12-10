/*
 * ASSERTION:
 *	To verify that a process can have and call a usdt probe with arguments
 *	and that those arguments are valid
 */

#pragma D option quiet

test_provider$1:::go
/arg0 == 42 && arg1 == 43 && arg2 == 44 /
{
	exit(0);
}
test_provider$1:::go
/arg0 != 42 || arg1 != 43 || arg2 != 44 /
{
	exit(1);
}
