
int g = 0;
int* gPtr = &g;

int foo() {
	return *gPtr;
}
