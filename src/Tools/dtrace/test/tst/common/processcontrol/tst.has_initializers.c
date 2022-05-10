#include <stdio.h>

void exported_library_function();

void main_binary_function()
{
	
}

int main(void)
{
	printf("reached main\n");

	main_binary_function();

	exported_library_function();

	return 0;
}
