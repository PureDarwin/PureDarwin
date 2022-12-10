#include <stdio.h>
#include <unistd.h>

extern void nop(void);

int waiting(volatile int *a)
{
	return (*a);
}

int main(void)
{
	volatile int a = 0;

	while (waiting(&a) == 0)
		continue;

	nop();

	return 0;
}

