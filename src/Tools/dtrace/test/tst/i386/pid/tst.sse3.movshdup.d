/*
 * ASSERTION : To test that SSE3 instructions can be instrumented by the pid
 * provider: If SSE3 instructions are not handled by dtrace, the return probe
 * for "multiply" will not be present
 *
 * SECTION: pid provider, x86_64
 *
 */

BEGIN, pid$1:a.out:multiply:return {
	exit(0);
}

