
__attribute__((weak))
int weakValue = 0;

int foo() {
	return weakValue;
}
