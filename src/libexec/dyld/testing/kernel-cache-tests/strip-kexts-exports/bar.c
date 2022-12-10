
extern int foo();
static int y;

int bar() {
	return foo() + y;
}