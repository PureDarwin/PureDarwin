#include <unistd.h>
#include <dlfcn.h>

int waiting(volatile int *a)
{
	return (*a);
}

/*
 * Value taken from pcre.h
 */
#define PCRE_CONFIG_UTF8 0

int main(void)
{
	volatile int a = 0;
	
	while (waiting(&a) == 0)
		continue;
	
	void* library = dlopen("/usr/lib/libpcre.dylib", RTLD_LAZY);
	int (*pcre_config)(int, void *) = (int (*)(int, void *))dlsym(library, "pcre_config");
	if (pcre_config) {
		int value;
		pcre_config(PCRE_CONFIG_UTF8, &value);
	}	
        dlclose(library);
	
	return 0;
}
