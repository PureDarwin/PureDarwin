#include <vector>
#include <unistd.h>

void f(std::vector<int> t) {
	return;
}

int main(void) {
	std::vector<int> v;
	while (1) {
		f(v);
		usleep(1);
	}

	return 0;
}
