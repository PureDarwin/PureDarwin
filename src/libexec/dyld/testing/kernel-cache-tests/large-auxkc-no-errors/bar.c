
__attribute__((used))
char largeBuffer[512 * 1024 * 1024];

extern int foo();

int bar() {
	return foo();
}