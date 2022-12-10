#include <unistd.h>
#include <test_provider.h>
int main(void) {
	if (fork() == 0) {
		TEST_PROVIDER_CALLED_AFTER_FORK();
	}
	return 0;
}

