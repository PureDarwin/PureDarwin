#include <test_provider.h>
#include <unistd.h>
int main(void) {
	while (1) {
		TEST_PROVIDER_GO(42, 43, 44);
		usleep(1);
	}
	return 0;
}
