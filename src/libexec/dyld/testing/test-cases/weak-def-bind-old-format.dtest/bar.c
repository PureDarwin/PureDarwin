
__attribute__((weak))
int weakTestValue = 42;

int bar() {
	return weakTestValue;
}