
int g = 0;

int bar() {
	return g;
}

__attribute__((section(("__TEXT, __text"))))
__typeof(&bar) barPtr = &bar;

int baz() {
	return barPtr();
}
