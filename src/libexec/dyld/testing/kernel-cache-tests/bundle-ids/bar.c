
int g = 0;

int bar() {
	return g;
}

__typeof(&bar) barPtr = &bar;

int baz() {
	return barPtr();
}
