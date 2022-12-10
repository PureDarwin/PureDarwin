
int x = 0;
int *p = &x;

extern int foo();

int bar() {
	return foo() + *p;
}
