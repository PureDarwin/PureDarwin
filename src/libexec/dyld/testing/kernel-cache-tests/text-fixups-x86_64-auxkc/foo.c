
int g = 0;

__attribute__((section(("__TEXT, __text"))))
int* gPtr = &g;

int foo() {
	return *gPtr;
}
