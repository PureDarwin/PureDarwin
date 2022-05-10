#include <unistd.h>

void f() {

}

int main(void) {
	while (1) {
		f();
		usleep(1);
	}
}
