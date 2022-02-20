
__attribute__((weak))
int weakValue = 0;

int bar() {
	return weakValue;
}