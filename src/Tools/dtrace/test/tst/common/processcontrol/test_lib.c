#include <stdio.h>

void function_called_by_initializer()
{
	printf("test_lib initializer ran\n");
}

void exported_library_function()
{
	
}

__attribute__((constructor))
static void
test_lib_initializer()
{
	function_called_by_initializer();
}
