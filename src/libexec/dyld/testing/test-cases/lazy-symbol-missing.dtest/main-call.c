

#include <stdio.h>

#ifdef WEAK
__attribute__((weak_import))
extern int slipperySymbol();
#else
extern int slipperySymbol();
#endif


int main(int argc, const char* argv[])
{
    slipperySymbol();
	return 0;
}

