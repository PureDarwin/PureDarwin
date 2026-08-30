#include <darwintest.h>
#include <stdio.h>

T_DECL(nox86exec_helper, "x86_64 binary that nox86exec test attempts to spawn")
{
	printf("Hello, Rosetta!");
	T_SKIP("I'm just a helper, in the world. That's all that you'll let me be.");
}
