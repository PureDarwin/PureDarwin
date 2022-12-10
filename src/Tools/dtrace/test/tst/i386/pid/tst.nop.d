
/*
 * Validate the ability to instrument a variety of NOP
 * instructions. This covers 1 - 10 byte NOPs, including
 * those with segment override prefixes.
 */

#pragma D option destructive

pid$1:a.out:waiting:entry
{
	this->value = (int *)alloca(sizeof (int));
	*this->value = 1;
	copyout(this->value, arg0, sizeof (int));
}

/*
 * This is a bit hacky, but...
 * The target method is just a bunch of NOPs.
 * We know that even optimized, the prologue is
 * two instructions, a push and mov. These are the
 * same size in i386 & x86_64. We start instrumenting
 * the NOPs following and count how many we hit.
 */

/* 1 byte NOP */
pid$1:a.out:nop:0
{
	/* This defaults to zero */
	self->nopCount++;
}

/* 2 byte NOP */
pid$1:a.out:nop:1
{
	self->nopCount++;
}

/* 3 byte NOP */
pid$1:a.out:nop:3
{
	self->nopCount++;
}

/* 4 byte NOP */
pid$1:a.out:nop:6
{
	self->nopCount++;
}

/* 5 byte NOP */
pid$1:a.out:nop:a
{
	self->nopCount++;
}

/* 6 byte NOP */
pid$1:a.out:nop:f
{
	self->nopCount++;
}

/* 7 byte NOP */
pid$1:a.out:nop:15
{
	self->nopCount++;
}

/* 8 byte NOP */
pid$1:a.out:nop:1c
{
	self->nopCount++;
}

/* 9 byte NOP */
pid$1:a.out:nop:24
{
	self->nopCount++;
}

/* 10 byte NOP, CS segment override prefix */
pid$1:a.out:nop:2d
{
	self->nopCount++;
}

/* 10 byte NOP, SS segment override prefix */
pid$1:a.out:nop:37
{
	self->nopCount++;
}

/* 10 byte NOP, DS segment override prefix */
pid$1:a.out:nop:41
{
	self->nopCount++;
}

/* 10 byte NOP, ES segment override prefix */
pid$1:a.out:nop:4b
{
	self->nopCount++;
}

/* 10 byte NOP, FS segment override prefix */
pid$1:a.out:nop:55
{
	self->nopCount++;
}

/* 10 byte NOP, GS segment override prefix */
pid$1:a.out:nop:5f
{
	self->nopCount++;
}

pid$1:a.out:nop:return
/ self->nopCount == 15 /
{
	/* Success, test passes */
	exit(0);
}

pid$1:a.out:nop:return
{
	/* FAIL */
	printf("Test matched %d NOPs, expected 15.\n", self->nopCount);
	exit(1);
}

BEGIN
{
	/*
	 * Let's just do this for 5 seconds.
	 */
	timeout = timestamp + 5000000000;
}

profile:::tick-4
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}

