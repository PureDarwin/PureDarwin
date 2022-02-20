
int g = 2;

int foo() {
	return g;
}

__typeof(&foo) fooPtr = &foo;

__attribute__((section(("__HIB, __data"))))
int f = 1;

__attribute__((section(("__HIB, __data"))))
int* fPtr  = &f;

__attribute__((section(("__HIB, __text"))))
int _start() {
	return f + fooPtr();
}
