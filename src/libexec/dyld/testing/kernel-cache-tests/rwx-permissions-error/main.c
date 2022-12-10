
__attribute__((section(("__RWX, __data"))))
int data = 1;

int _start() {
	return data;
}