
int pack = 0;

extern int foo();

int bar() {
	return foo() + pack;
}