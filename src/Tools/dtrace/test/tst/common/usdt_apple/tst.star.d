/*
 * ASSERTION:
 *	To verify that test_provider* is a valid syntax for USDT probes to
 *	start tracing on every process for this provider
 */

#pragma D option quiet

test_provider*:::go
/arg0 == 42 && arg1 == 43 && arg2 == 44 /
{
	exit(0);
}
test_provider*:::go
/arg0 != 42 || arg1 != 43 || arg2 != 44 /
{
	exit(1);
}

/* (Prevents DTrace from issuing a compilation error because we did not use $1) */
test_provider$1:::go
{

}
