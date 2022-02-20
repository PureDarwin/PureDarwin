
extern int foo();
extern int f;

__typeof(&foo) fooPtr = &foo;

int bar() {
	return foo() + fooPtr() + f;
}